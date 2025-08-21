#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include "animation/AvatarDefinition.h"
#include "ecs/Entity.h" // assumes EntityID typedef lives there; adjust path if different
#include "pipeline/AssetReference.h" // for ClaymoreGUID

// ------------ Blend Shapes ------------
struct BlendShape {
    std::string Name;
    std::vector<glm::vec3> DeltaPos;
    std::vector<glm::vec3> DeltaNormal;
    float Weight = 0.0f;
};

struct BlendShapeComponent {
    std::vector<BlendShape> Shapes;
    bool Dirty = false;
};

// ------------ Skeleton & Skinning ------------
struct SkeletonComponent {
    std::vector<glm::mat4> InverseBindPoses;  // inverse bind matrices per bone
    std::vector<glm::mat4> BindPoseGlobals;

    std::vector<EntityID>  BoneEntities;      // entity for each bone (index matches InverseBindPoses)

    // Name → index lookup to enable fast sampling & editor display.
    std::unordered_map<std::string, int> BoneNameToIndex;
    std::vector<int> BoneParents; // index of parent bone (-1 for root)

    // Optional: stable names aligned by index (authoring or import time). If empty, derive from BoneNameToIndex.
    std::vector<std::string> BoneNames;

    // Optional: stable skeleton asset GUID and per-joint GUIDs (Hash64(skeletonGuid + "/" + fullPath))
    ClaymoreGUID SkeletonGuid; // zero when unknown
    std::vector<uint64_t> JointGuids; // size == number of bones when computed

    int GetBoneIndex(const std::string& name) const {
        auto it = BoneNameToIndex.find(name);
        return it != BoneNameToIndex.end() ? it->second : -1;
    }

    // Optional humanoid avatar built for this skeleton
    std::unique_ptr<cm::animation::AvatarDefinition> Avatar;

};

struct SkinningComponent {
    EntityID SkeletonRoot = -1;
    std::vector<glm::mat4> Palette;           // current frame palette
};
