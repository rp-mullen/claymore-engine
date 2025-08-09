#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include "animation/AnimationTypes.h"
#include "animation/AnimatorRuntime.h"

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
    // Optional: Animator controller
    std::string ControllerPath; // .animctrl JSON file

    // Runtime controller & animator
    std::shared_ptr<AnimatorController> Controller;
    Animator AnimatorInstance;
    int CurrentStateId = -1;
    // Cache of loaded clips per state id
    std::unordered_map<int, std::shared_ptr<AnimationClip>> CachedClips;
};

} // namespace animation
} // namespace cm
