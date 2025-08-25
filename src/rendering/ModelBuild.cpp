#include "rendering/ModelBuild.h"
#include "pipeline/AssetLibrary.h"
#include "rendering/MaterialManager.h"
#include "rendering/SkinnedPBRMaterial.h"
#include "animation/SkeletonBinding.h"
#include <iostream>

static inline bool IsZeroGuid(const ClaymoreGUID& g) { return g.high == 0 && g.low == 0; }

BuildResult BuildRendererFromAssets(const BuildModelParams& p)
{
    BuildResult result{};
    if (!p.scene || p.entity == (EntityID)-1) {
        std::cerr << "[ModelBuild] ERROR: Invalid scene/entity for build." << std::endl;
        return result;
    }

    EntityData* data = p.scene->GetEntityData(p.entity);
    if (!data) {
        std::cerr << "[ModelBuild] ERROR: Entity not found: " << p.entity << std::endl;
        return result;
    }

    // Ensure MeshComponent exists and mesh is loaded
    if (!data->Mesh) data->Mesh = std::make_unique<MeshComponent>();

    // Load mesh by AssetReference if not already present
    if (!data->Mesh->mesh) {
        if (IsZeroGuid(p.meshGuid)) {
            std::cerr << "[ModelBuild] ERROR: meshGuid is missing." << std::endl;
            return result;
        }
        AssetReference meshRef(p.meshGuid, p.meshFileId, (int)AssetType::Mesh);
        data->Mesh->meshReference = meshRef;
        data->Mesh->mesh = AssetLibrary::Instance().LoadMesh(meshRef);
        if (!data->Mesh->mesh) {
            std::cerr << "[ModelBuild] ERROR: Failed to load mesh for GUID " << p.meshGuid.ToString() << " fileID=" << p.meshFileId << std::endl;
            return result;
        }
    }

    Mesh* meshPtr = data->Mesh->mesh.get();
    const bool meshIsSkinned = meshPtr && meshPtr->HasSkinning();
    result.isSkinned = meshIsSkinned;

    // Classify and enforce contract: if skinned, we require a skeleton in hierarchy
    if (meshIsSkinned) {
        // Find nearest ancestor SkeletonComponent
        EntityID cur = data->Parent;
        EntityID foundSkeletonRoot = (EntityID)-1;
        SkeletonComponent* foundSkel = nullptr;
        size_t guard = 0;
        while (cur != (EntityID)-1 && guard++ < 200000) {
            EntityData* pd = p.scene->GetEntityData(cur);
            if (!pd) break;
            if (pd->Skeleton) { foundSkeletonRoot = cur; foundSkel = pd->Skeleton.get(); break; }
            cur = pd->Parent;
        }

        if (!foundSkel) {
            std::cerr << "[ModelBuild] ERROR: Skinned mesh requires a skeleton ancestor; none found for entity '" << data->Name << "'." << std::endl;
            return result;
        }
        // If a skeletonGuid was provided, validate it
        if (!IsZeroGuid(p.skeletonGuid)) {
            if (foundSkel->SkeletonGuid.high != p.skeletonGuid.high || foundSkel->SkeletonGuid.low != p.skeletonGuid.low) {
                std::cerr << "[ModelBuild] ERROR: Skinned mesh skeleton GUID mismatch for entity '" << data->Name << "'." << std::endl;
                return result;
            }
        }

        // Ensure SkinningComponent exists and is bound to this skeleton root
        if (!data->Skinning) data->Skinning = std::make_unique<SkinningComponent>();
        data->Skinning->SkeletonRoot = foundSkeletonRoot;

        // Ensure skinned PBR material is used
        if (!std::dynamic_pointer_cast<SkinnedPBRMaterial>(data->Mesh->material)) {
            data->Mesh->material = MaterialManager::Instance().CreateSceneSkinnedDefaultMaterial(&Scene::Get());
        }

        // Build remap and used-joint list using current mesh and skeleton
        std::vector<uint16_t> remap, used;
        if (!BuildBoneRemap(*meshPtr, *foundSkel, remap, used)) {
            // Mesh had no skinning or mismatch; treat as error to avoid silent downgrade
            std::cerr << "[ModelBuild] ERROR: Failed to build bone remap for skinned mesh on entity '" << data->Name << "'." << std::endl;
            return result;
        }
        result.usedJointList = used;
        result.remap = remap;

        // Initialize palette to bind pose; sized to max joints or used joints
        const size_t paletteSize = std::min(used.size(), (size_t)SkinnedPBRMaterial::MaxBones);
        data->Skinning->Palette.assign(paletteSize, glm::mat4(1.0f));

        result.ok = true;
        return result;
    }

    // Static path: ensure we have a default PBR material if none
    if (!data->Mesh->material) {
        data->Mesh->material = MaterialManager::Instance().CreateSceneDefaultMaterial(&Scene::Get());
    }

    // Ensure no stale SkinningComponent remains on static meshes
    if (data->Skinning) {
        data->Skinning.reset();
    }

    result.ok = true;
    return result;
}


