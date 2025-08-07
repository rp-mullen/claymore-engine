#include "SkinningSystem.h"
#include "ecs/AnimationComponents.h"
#include "rendering/VertexTypes.h"
#include "rendering/SkinnedPBRMaterial.h"
#include <unordered_map>
#include <bgfx/bgfx.h>

void SkinningSystem::Update(Scene& scene)
{
    // Build a quick map from entity id to WorldMatrix for bones
    auto& entities = scene.GetEntities();
    std::unordered_map<EntityID,const glm::mat4*> worldCache;
    for (const auto& ent : entities) {
        auto* data = scene.GetEntityData(ent.GetID());
        if (data)
            worldCache[ent.GetID()] = &data->Transform.WorldMatrix;
    }

    for (const auto& ent : entities) {
        auto* data = scene.GetEntityData(ent.GetID());
        if (!data || !data->Mesh) continue;

        // ------------- Skinning (if the mesh has bones) -----------------
        if (data->Skinning) {
            auto* skin = data->Skinning;
            auto* skeletonData = scene.GetEntityData(skin->SkeletonRoot);
            if (skeletonData && skeletonData->Skeleton) {
                const auto& invBind      = skeletonData->Skeleton->InverseBindPoses;
                const auto& boneEntities = skeletonData->Skeleton->BoneEntities;
                size_t boneCount = invBind.size();
                skin->Palette.resize(boneCount);
                for (size_t i = 0; i < boneCount; ++i) {
                    EntityID bid = boneEntities[i];
                    auto it = worldCache.find(bid);
                    glm::mat4 boneWorld = it != worldCache.end() ? *it->second : glm::mat4(1.0f);

                    // Convert bone world into model-space to prevent double-transform when applying u_model in shader.
                    // We assume the mesh entity's parent is the model root. Use that world as model matrix.
                    glm::mat4 modelWorld = glm::mat4(1.0f);
                    if (auto* meshData = data) {
                        modelWorld = meshData->Transform.WorldMatrix;
                    }
                    glm::mat4 modelWorldInv = glm::inverse(modelWorld);

                    // Final skin matrix maps from bind pose to current pose in model space
                    skin->Palette[i] = modelWorldInv * boneWorld * invBind[i];
                }

                if (auto skMat = std::dynamic_pointer_cast<SkinnedPBRMaterial>(data->Mesh->material)) {
                    skMat->UploadBones(skin->Palette);
                }
            }
        }

        // ------------- CPU Blend-shape deformation ----------------------
        auto meshPtr = data->Mesh->mesh.get();
        if (data->BlendShapes && meshPtr && meshPtr->Dynamic && data->Mesh->BlendShapes->Dirty) {
            const auto& bsComp = *data->BlendShapes;
            size_t vCount = meshPtr->Vertices.size();
            std::vector<PBRVertex> blended(vCount);

            // Base
            for (size_t vi = 0; vi < vCount; ++vi) {
                blended[vi].x  = meshPtr->Vertices[vi].x;
                blended[vi].y  = meshPtr->Vertices[vi].y;
                blended[vi].z  = meshPtr->Vertices[vi].z;
                blended[vi].nx = meshPtr->Normals[vi].x;
                blended[vi].ny = meshPtr->Normals[vi].y;
                blended[vi].nz = meshPtr->Normals[vi].z;
                blended[vi].u  = 0; blended[vi].v = 0;
            }

            // Apply each active shape
            for (const auto& shape : bsComp.Shapes) {
                float w = shape.Weight;
                if (w == 0.0f) continue;
                for (size_t vi = 0; vi < vCount; ++vi) {
                    blended[vi].x  += shape.DeltaPos   [vi].x * w;
                    blended[vi].y  += shape.DeltaPos   [vi].y * w;
                    blended[vi].z  += shape.DeltaPos   [vi].z * w;
                    blended[vi].nx += shape.DeltaNormal[vi].x * w;
                    blended[vi].ny += shape.DeltaNormal[vi].y * w;
                    blended[vi].nz += shape.DeltaNormal[vi].z * w;
                }
            }

            const bgfx::Memory* mem = bgfx::copy(blended.data(), uint32_t(sizeof(PBRVertex)*blended.size()));
            bgfx::update(meshPtr->dvbh, 0, mem);
            data->BlendShapes->Dirty = false;
        }
    }
}
