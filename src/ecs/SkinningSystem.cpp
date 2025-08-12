#include "SkinningSystem.h"
#include "ecs/AnimationComponents.h"
#include "rendering/VertexTypes.h"
#include "rendering/SkinnedPBRMaterial.h"
#include <unordered_map>
#include <bgfx/bgfx.h>

static inline glm::mat4 GetWorldOrIdentity(Scene& scene, EntityID id)
{
	auto* data = scene.GetEntityData(id);
	if (data)
	{
		return data->Transform.WorldMatrix;
	}
	return glm::mat4(1.0f);
}

void SkinningSystem::Update(Scene& scene)
{
    // Build a quick map from entity id to WorldMatrix for bones
    auto& entities = scene.GetEntities();
    std::unordered_map<EntityID, const glm::mat4*> worldCache;
    for (const auto& ent : entities) {
        auto* data = scene.GetEntityData(ent.GetID());
        if (data)
            worldCache[ent.GetID()] = &data->Transform.WorldMatrix;
    }

    for (const auto& ent : entities) {
        auto* data = scene.GetEntityData(ent.GetID());
        if (!data || !data->Mesh) continue;

        // Must be skinned
        if (!data->Skinning) continue;
        auto* skin = data->Skinning;

        // Resolve skeleton root/entity
        if (skin->SkeletonRoot == (EntityID)-1) continue;
        auto* skeletonData = scene.GetEntityData(skin->SkeletonRoot);
        if (!skeletonData || !skeletonData->Skeleton) continue;

        const SkeletonComponent& skel = *skeletonData->Skeleton;

        // Nothing to do if skeleton has no bones
        const size_t boneCountRaw =
            std::min(skel.InverseBindPoses.size(), skel.BoneEntities.size());
        if (boneCountRaw == 0) continue;

        // Clamp to shader palette capacity
        const size_t boneCount =
            std::min(boneCountRaw, (size_t)SkinnedPBRMaterial::MaxBones);

        // Ensure palette sized correctly
        if (skin->Palette.size() != boneCount)
            skin->Palette.assign(boneCount, glm::mat4(1.0f));

        // Mesh space transform (palette must be in mesh-local space because shader multiplies by u_model)
        const glm::mat4 meshWorld = data->Transform.WorldMatrix;
        const glm::mat4 invMeshWorld = glm::inverse(meshWorld);


        // Build palette in mesh local space (correct bind handling)
        const bool inBindPose =
            (!skeletonData->AnimationPlayer) ||
            (skeletonData->AnimationPlayer->ActiveStates.empty()) ||
            std::all_of(skeletonData->AnimationPlayer->ActiveStates.begin(),
                skeletonData->AnimationPlayer->ActiveStates.end(),
                [](const auto& s) { return s.Weight <= 0.0f; });

        for (size_t i = 0; i < boneCount; ++i)
        {
            const glm::mat4 invBind = skel.InverseBindPoses[i];

            // Reconstruct exact bind bone WORLD:
            // InvBind = inverse(B_bind) * M_bind  =>  B_bind = M_bind * inverse(InvBind)
            const glm::mat4 boneWorld =
                inBindPose
                ? (meshWorld * glm::inverse(invBind))           // exact bind world
                : GetWorldOrIdentity(scene, skel.BoneEntities[i]);

            // Palette in mesh-local:
            // P = inverse(M_current) * B_world * InvBind
            skin->Palette[i] = invMeshWorld * boneWorld * invBind;
        }

        // Upload to GPU if this mesh uses a skinned PBR material
        if (auto skMat = std::dynamic_pointer_cast<SkinnedPBRMaterial>(data->Mesh->material)) {
            skMat->UploadBones(skin->Palette);
        }



        // ------------- CPU Blend-shape deformation ----------------------
        auto meshPtr = data->Mesh->mesh.get();
        if (data->BlendShapes && meshPtr && meshPtr->Dynamic && data->Mesh->BlendShapes->Dirty)
        {
            const auto& bsComp = *data->BlendShapes;
            const size_t vCount = meshPtr->Vertices.size();

            if (vCount == 0) {
                data->BlendShapes->Dirty = false;
                continue;
            }

            // Determine if this mesh uses skinned layout by peeking the material type.
            const bool isSkinnedVB = (bool)std::dynamic_pointer_cast<SkinnedPBRMaterial>(data->Mesh->material);

            if (isSkinnedVB)
            {
                // Build a SkinnedPBRVertex buffer (pos/nrm morphed; uv/indices/weights preserved)
                std::vector<SkinnedPBRVertex> blended(vCount);

                // Base copy
                for (size_t vi = 0; vi < vCount; ++vi)
                {
                    blended[vi].x = meshPtr->Vertices[vi].x;
                    blended[vi].y = meshPtr->Vertices[vi].y;
                    blended[vi].z = meshPtr->Vertices[vi].z;
                    blended[vi].nx = meshPtr->Normals[vi].x;
                    blended[vi].ny = meshPtr->Normals[vi].y;
                    blended[vi].nz = meshPtr->Normals[vi].z;

                    // Preserve UVs, bone indices, and weights from the CURRENT GPU layout source of truth.
                    // If you keep CPU copies, read them here; otherwise cache them when you created the mesh.
                    // Assuming Mesh stores UVs/Weights/Indices as CPU-side arrays:
                    blended[vi].u = 0.0f;
                    blended[vi].v = 0.0f;

                    const glm::ivec4 bi = (vi < meshPtr->BoneIndices.size()) ? meshPtr->BoneIndices[vi] : glm::ivec4(0);
                    blended[vi].i0 = (uint8_t)bi.x; blended[vi].i1 = (uint8_t)bi.y;
                    blended[vi].i2 = (uint8_t)bi.z; blended[vi].i3 = (uint8_t)bi.w;

                    const glm::vec4 bw = (vi < meshPtr->BoneWeights.size()) ? meshPtr->BoneWeights[vi] : glm::vec4(1, 0, 0, 0);
                    blended[vi].w0 = bw.x; blended[vi].w1 = bw.y; blended[vi].w2 = bw.z; blended[vi].w3 = bw.w;
                }

                // Apply each active shape
                for (const auto& shape : bsComp.Shapes)
                {
                    const float w = shape.Weight;
                    if (w == 0.0f) continue;

                    // Safety: match vertex counts
                    if (shape.DeltaPos.size() != vCount || shape.DeltaNormal.size() != vCount) continue;

                    for (size_t vi = 0; vi < vCount; ++vi)
                    {
                        blended[vi].x += shape.DeltaPos[vi].x * w;
                        blended[vi].y += shape.DeltaPos[vi].y * w;
                        blended[vi].z += shape.DeltaPos[vi].z * w;
                        blended[vi].nx += shape.DeltaNormal[vi].x * w;
                        blended[vi].ny += shape.DeltaNormal[vi].y * w;
                        blended[vi].nz += shape.DeltaNormal[vi].z * w;
                    }
                }

                const bgfx::Memory* mem = bgfx::copy(blended.data(), uint32_t(sizeof(SkinnedPBRVertex) * blended.size()));
                bgfx::update(meshPtr->dvbh, 0, mem);
            }
            else
            {
                // Non-skinned dynamic mesh: use PBRVertex
                std::vector<PBRVertex> blended(vCount);

                for (size_t vi = 0; vi < vCount; ++vi)
                {
                    blended[vi].x = meshPtr->Vertices[vi].x;
                    blended[vi].y = meshPtr->Vertices[vi].y;
                    blended[vi].z = meshPtr->Vertices[vi].z;
                    blended[vi].nx = meshPtr->Normals[vi].x;
                    blended[vi].ny = meshPtr->Normals[vi].y;
                    blended[vi].nz = meshPtr->Normals[vi].z;

                    blended[vi].u = 0.0f;
                    blended[vi].v = 0.0f;
                }

                for (const auto& shape : bsComp.Shapes)
                {
                    const float w = shape.Weight;
                    if (w == 0.0f) continue;
                    if (shape.DeltaPos.size() != vCount || shape.DeltaNormal.size() != vCount) continue;

                    for (size_t vi = 0; vi < vCount; ++vi)
                    {
                        blended[vi].x += shape.DeltaPos[vi].x * w;
                        blended[vi].y += shape.DeltaPos[vi].y * w;
                        blended[vi].z += shape.DeltaPos[vi].z * w;
                        blended[vi].nx += shape.DeltaNormal[vi].x * w;
                        blended[vi].ny += shape.DeltaNormal[vi].y * w;
                        blended[vi].nz += shape.DeltaNormal[vi].z * w;
                    }
                }

                const bgfx::Memory* mem = bgfx::copy(blended.data(), uint32_t(sizeof(PBRVertex) * blended.size()));
                bgfx::update(meshPtr->dvbh, 0, mem);
            }

            data->BlendShapes->Dirty = false;
        }

    }
}