#pragma once

#include <string>
#include <vector>
#include "animation/AnimationTypes.h"

namespace cm {
namespace animation {

// -----------------------------
// Generic property animation track (float-only for now)
// -----------------------------
struct PropertyTrack {
    std::string PropertyPath;                // e.g. "Transform.Position.x"
    std::vector<KeyframeFloat> Keys;
};

struct TimelineClip {
    std::string Name;
    float Length = 0.0f;                     // Seconds
    std::vector<PropertyTrack> Tracks;
};

} // namespace animation
} // namespace cm
