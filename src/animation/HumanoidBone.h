#pragma once

#include <cstdint>
#include <string>

namespace cm {
namespace animation {

// Canonical humanoid bone map (Unity Mecanim-like)
enum class HumanoidBone : uint16_t {
    Root = 0,
    Hips,
    Spine,
    Chest,
    UpperChest,
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
    LeftToes,

    RightUpperLeg,
    RightLowerLeg,
    RightFoot,
    RightToes,

    LeftEye,
    RightEye,

    // Fingers (optional)
    LeftThumbProx,
    LeftThumbInter,
    LeftThumbDist,
    LeftIndexProx,
    LeftIndexInter,
    LeftIndexDist,
    LeftMiddleProx,
    LeftMiddleInter,
    LeftMiddleDist,
    LeftRingProx,
    LeftRingInter,
    LeftRingDist,
    LeftLittleProx,
    LeftLittleInter,
    LeftLittleDist,

    RightThumbProx,
    RightThumbInter,
    RightThumbDist,
    RightIndexProx,
    RightIndexInter,
    RightIndexDist,
    RightMiddleProx,
    RightMiddleInter,
    RightMiddleDist,
    RightRingProx,
    RightRingInter,
    RightRingDist,
    RightLittleProx,
    RightLittleInter,
    RightLittleDist,

    // Twists (helpers, optional)
    LeftUpperArmTwist,
    LeftLowerArmTwist,
    RightUpperArmTwist,
    RightLowerArmTwist,
    LeftUpperLegTwist,
    LeftLowerLegTwist,
    RightUpperLegTwist,
    RightLowerLegTwist,

    Count
};

constexpr uint16_t HumanoidBoneCount = static_cast<uint16_t>(HumanoidBone::Count);

inline const char* ToString(HumanoidBone b)
{
    switch (b) {
        case HumanoidBone::Root: return "Root";
        case HumanoidBone::Hips: return "Hips";
        case HumanoidBone::Spine: return "Spine";
        case HumanoidBone::Chest: return "Chest";
        case HumanoidBone::UpperChest: return "UpperChest";
        case HumanoidBone::Neck: return "Neck";
        case HumanoidBone::Head: return "Head";
        case HumanoidBone::LeftShoulder: return "LeftShoulder";
        case HumanoidBone::LeftUpperArm: return "LeftUpperArm";
        case HumanoidBone::LeftLowerArm: return "LeftLowerArm";
        case HumanoidBone::LeftHand: return "LeftHand";
        case HumanoidBone::RightShoulder: return "RightShoulder";
        case HumanoidBone::RightUpperArm: return "RightUpperArm";
        case HumanoidBone::RightLowerArm: return "RightLowerArm";
        case HumanoidBone::RightHand: return "RightHand";
        case HumanoidBone::LeftUpperLeg: return "LeftUpperLeg";
        case HumanoidBone::LeftLowerLeg: return "LeftLowerLeg";
        case HumanoidBone::LeftFoot: return "LeftFoot";
        case HumanoidBone::LeftToes: return "LeftToes";
        case HumanoidBone::RightUpperLeg: return "RightUpperLeg";
        case HumanoidBone::RightLowerLeg: return "RightLowerLeg";
        case HumanoidBone::RightFoot: return "RightFoot";
        case HumanoidBone::RightToes: return "RightToes";
        case HumanoidBone::LeftEye: return "LeftEye";
        case HumanoidBone::RightEye: return "RightEye";
        case HumanoidBone::LeftThumbProx: return "LeftThumbProx";
        case HumanoidBone::LeftThumbInter: return "LeftThumbInter";
        case HumanoidBone::LeftThumbDist: return "LeftThumbDist";
        case HumanoidBone::LeftIndexProx: return "LeftIndexProx";
        case HumanoidBone::LeftIndexInter: return "LeftIndexInter";
        case HumanoidBone::LeftIndexDist: return "LeftIndexDist";
        case HumanoidBone::LeftMiddleProx: return "LeftMiddleProx";
        case HumanoidBone::LeftMiddleInter: return "LeftMiddleInter";
        case HumanoidBone::LeftMiddleDist: return "LeftMiddleDist";
        case HumanoidBone::LeftRingProx: return "LeftRingProx";
        case HumanoidBone::LeftRingInter: return "LeftRingInter";
        case HumanoidBone::LeftRingDist: return "LeftRingDist";
        case HumanoidBone::LeftLittleProx: return "LeftLittleProx";
        case HumanoidBone::LeftLittleInter: return "LeftLittleInter";
        case HumanoidBone::LeftLittleDist: return "LeftLittleDist";
        case HumanoidBone::RightThumbProx: return "RightThumbProx";
        case HumanoidBone::RightThumbInter: return "RightThumbInter";
        case HumanoidBone::RightThumbDist: return "RightThumbDist";
        case HumanoidBone::RightIndexProx: return "RightIndexProx";
        case HumanoidBone::RightIndexInter: return "RightIndexInter";
        case HumanoidBone::RightIndexDist: return "RightIndexDist";
        case HumanoidBone::RightMiddleProx: return "RightMiddleProx";
        case HumanoidBone::RightMiddleInter: return "RightMiddleInter";
        case HumanoidBone::RightMiddleDist: return "RightMiddleDist";
        case HumanoidBone::RightRingProx: return "RightRingProx";
        case HumanoidBone::RightRingInter: return "RightRingInter";
        case HumanoidBone::RightRingDist: return "RightRingDist";
        case HumanoidBone::RightLittleProx: return "RightLittleProx";
        case HumanoidBone::RightLittleInter: return "RightLittleInter";
        case HumanoidBone::RightLittleDist: return "RightLittleDist";
        case HumanoidBone::LeftUpperArmTwist: return "LeftUpperArmTwist";
        case HumanoidBone::LeftLowerArmTwist: return "LeftLowerArmTwist";
        case HumanoidBone::RightUpperArmTwist: return "RightUpperArmTwist";
        case HumanoidBone::RightLowerArmTwist: return "RightLowerArmTwist";
        case HumanoidBone::LeftUpperLegTwist: return "LeftUpperLegTwist";
        case HumanoidBone::LeftLowerLegTwist: return "LeftLowerLegTwist";
        case HumanoidBone::RightUpperLegTwist: return "RightUpperLegTwist";
        case HumanoidBone::RightLowerLegTwist: return "RightLowerLegTwist";
        case HumanoidBone::Count: return "Count";
        default: return "Unknown";
    }
}

inline bool IsHumanoidBoneRequired(HumanoidBone b)
{
    switch (b) {
        case HumanoidBone::Hips:
        case HumanoidBone::Spine:
        case HumanoidBone::Neck: // Neck or Head
        case HumanoidBone::Head:
        case HumanoidBone::LeftUpperArm:
        case HumanoidBone::LeftLowerArm:
        case HumanoidBone::LeftHand:
        case HumanoidBone::RightUpperArm:
        case HumanoidBone::RightLowerArm:
        case HumanoidBone::RightHand:
        case HumanoidBone::LeftUpperLeg:
        case HumanoidBone::LeftLowerLeg:
        case HumanoidBone::LeftFoot:
        case HumanoidBone::RightUpperLeg:
        case HumanoidBone::RightLowerLeg:
        case HumanoidBone::RightFoot:
            return true;
        default:
            return false;
    }
}

} // namespace animation
} // namespace cm


