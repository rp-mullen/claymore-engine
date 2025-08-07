#pragma once

#include <vector>
#include "animation/AnimationTypes.h"

namespace cm {
namespace animation {

struct AnimationState {
    const AnimationClip* Clip = nullptr; // Clip being played
    float Time = 0.0f;                   // Current playback time (seconds)
    float Weight = 1.0f;                 // Blend weight (0..1)
    bool Loop = true;
};

struct AnimationPlayerComponent {
    std::vector<AnimationState> ActiveStates;   // Multiple layers / states
    float PlaybackSpeed = 1.0f;
};

} // namespace animation
} // namespace cm
