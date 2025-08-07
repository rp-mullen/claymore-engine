#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace cm {
namespace animation {

// -----------------------------
// Keyframe data structures
// -----------------------------
struct KeyframeVec3 {
    float Time = 0.0f;          // Seconds from start of clip
    glm::vec3 Value{0.0f};      // Value at Time
};

struct KeyframeQuat {
    float Time = 0.0f;          // Seconds from start of clip
    glm::quat Value{1.0f, 0.0f, 0.0f, 0.0f};
};

struct KeyframeFloat {
    float Time = 0.0f;
    float Value = 0.0f;
};

// -----------------------------
// Bone animation track
// -----------------------------
struct BoneTrack {
    std::vector<KeyframeVec3> PositionKeys;
    std::vector<KeyframeQuat> RotationKeys;
    std::vector<KeyframeVec3> ScaleKeys;

    bool IsEmpty() const {
        return PositionKeys.empty() && RotationKeys.empty() && ScaleKeys.empty();
    }
};

// -----------------------------
// Full skeletal clip
// -----------------------------
struct AnimationClip {
    std::string Name;
    float Duration = 0.0f;                // Seconds
    float TicksPerSecond = 0.0f;          // Source ticks/sec (for FBX)

    // Map of skeleton bone name -> animated track
    std::unordered_map<std::string, BoneTrack> BoneTracks;
};

} // namespace animation
} // namespace cm
