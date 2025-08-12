#pragma once

#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Minimal curve utilities for animation sampling. Linear by default; can be
// extended with Hermite in future.

namespace cm {
namespace animation {

using KeyID = std::uint64_t;

struct KeyFloat { KeyID id = 0; float t = 0.0f; float v = 0.0f; };
struct KeyVec2  { KeyID id = 0; float t = 0.0f; glm::vec2 v{0.0f}; };
struct KeyVec3  { KeyID id = 0; float t = 0.0f; glm::vec3 v{0.0f}; };
struct KeyQuat  { KeyID id = 0; float t = 0.0f; glm::quat v{1,0,0,0}; };
struct KeyColor { KeyID id = 0; float t = 0.0f; glm::vec4 v{1.0f}; };

// Simple segment cache for monotonic key times
struct SegmentCache { mutable int last = 0; };

struct CurveFloat {
    std::vector<KeyFloat> keys;
    mutable SegmentCache cache;
    float Sample(float t, bool loop = false, float length = 0.0f) const;
};

struct CurveVec2 {
    std::vector<KeyVec2> keys; mutable SegmentCache cache;
    glm::vec2 Sample(float t, bool loop = false, float length = 0.0f) const;
};

struct CurveVec3 {
    std::vector<KeyVec3> keys; mutable SegmentCache cache;
    glm::vec3 Sample(float t, bool loop = false, float length = 0.0f) const;
};

struct CurveQuat {
    std::vector<KeyQuat> keys; mutable SegmentCache cache;
    glm::quat Sample(float t, bool loop = false, float length = 0.0f) const; // slerp
};

struct CurveColor {
    std::vector<KeyColor> keys; mutable SegmentCache cache;
    glm::vec4 Sample(float t, bool loop = false, float length = 0.0f) const;
};

} // namespace animation
} // namespace cm



