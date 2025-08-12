#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <optional>

struct SkeletonComponent; // forward

namespace cm {
namespace animation {

// List of standardized human bones inspired by Unity's Mecanim.
// Keeping it small for now – can be extended later.
enum class HumanBone : uint8_t {
    Hips,
    Spine,
    Chest,
    Neck,
    Head,

    LeftShoulder,
    LeftUpperArm,
    LeftLowerArm,
    LeftHand,

    RightShoulder,
    RightUpperArm,
    RightLowerArm,
    RightHand,

    LeftUpperLeg,
    LeftLowerLeg,
    LeftFoot,

    RightUpperLeg,
    RightLowerLeg,
    RightFoot,
};

inline const char* ToString(HumanBone bone) {
    switch (bone) {
        case HumanBone::Hips: return "Hips";
        case HumanBone::Spine: return "Spine";
        case HumanBone::Chest: return "Chest";
        case HumanBone::Neck: return "Neck";
        case HumanBone::Head: return "Head";
        case HumanBone::LeftShoulder: return "LeftShoulder";
        case HumanBone::LeftUpperArm: return "LeftUpperArm";
        case HumanBone::LeftLowerArm: return "LeftLowerArm";
        case HumanBone::LeftHand: return "LeftHand";
        case HumanBone::RightShoulder: return "RightShoulder";
        case HumanBone::RightUpperArm: return "RightUpperArm";
        case HumanBone::RightLowerArm: return "RightLowerArm";
        case HumanBone::RightHand: return "RightHand";
        case HumanBone::LeftUpperLeg: return "LeftUpperLeg";
        case HumanBone::LeftLowerLeg: return "LeftLowerLeg";
        case HumanBone::LeftFoot: return "LeftFoot";
        case HumanBone::RightUpperLeg: return "RightUpperLeg";
        case HumanBone::RightLowerLeg: return "RightLowerLeg";
        case HumanBone::RightFoot: return "RightFoot";
        default: return "Unknown";
    }
}

// Mapping from a standardized human bone to a skeleton bone (by name).
struct HumanoidAvatar {
    std::unordered_map<HumanBone, std::string> BoneMapping;

    // Convenience helpers
    std::optional<std::string> GetBoneName(HumanBone bone) const {
        auto it = BoneMapping.find(bone);
        if (it != BoneMapping.end()) return it->second;
        return std::nullopt;
    }

    // Map canonical human bone id to skeleton bone index using name resolution
    int HumanToSkeleton(int humanBoneId, const ::SkeletonComponent& skeleton) const;
};

} // namespace animation
} // namespace cm
