#pragma once
#include <vector>
#include <cstdint>
#include "pipeline/AssetReference.h" // ClaymoreGUID
#include "ecs/Scene.h"

// Shared builder used by both model import path and prefab instantiation
// to construct renderer-type components in a single, consistent way.

struct BuildModelParams {
    ClaymoreGUID meshGuid{};         // GUID of model asset (submesh selected via meshFileId)
    int32_t      meshFileId{0};      // Submesh index (fileID) for multi-mesh assets
    ClaymoreGUID skeletonGuid{};     // Required if skinned; zero for static
    const std::vector<ClaymoreGUID>* materialGuids{nullptr}; // Optional; not yet used (material assets WIP)
    EntityID     entity{(EntityID)-1};
    Scene*       scene{nullptr};
};

struct BuildResult {
    bool ok{false};
    bool isSkinned{false};
    std::vector<uint16_t> usedJointList; // compact list of joints used by this mesh
    std::vector<uint16_t> remap;         // mesh->skeleton index remap (identity by default)
};

// Creates/updates MeshComponent (and SkinningComponent for skinned) on the target entity.
// - Loads mesh from AssetLibrary using meshGuid/fileId unless the entity already has a loaded mesh
// - Chooses skinned vs static path based on actual mesh skinning data (no silent downgrade)
// - For skinned, finds nearest ancestor SkeletonComponent and validates skeleton GUID if provided
// - Builds usedJointList, remap and initializes/refreshes palette buffers to bind pose
// Returns BuildResult with details; on error, leaves components untouched and returns ok=false
BuildResult BuildRendererFromAssets(const BuildModelParams& params);


