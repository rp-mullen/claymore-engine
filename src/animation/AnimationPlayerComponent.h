#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <glm/glm.hpp>
#include "animation/AnimationTypes.h"
#include "animation/AnimatorRuntime.h"
#include "animation/AnimationAsset.h"

namespace cm {
namespace animation {

struct AnimationState {
    const AnimationClip* LegacyClip = nullptr;      // legacy
    const AnimationAsset* Asset = nullptr;          // unified
    float Time = 0.0f;                              // Current playback time (seconds)
    float Weight = 1.0f;                            // Blend weight (0..1)
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
    // Cache of loaded assets per state id
    std::unordered_map<int, std::shared_ptr<AnimationClip>> CachedClips; // legacy clips
    std::unordered_map<int, std::shared_ptr<AnimationAsset>> CachedAssets; // unified assets

    // Root motion handling
    enum class RootMotionMode { None, FromHipsToEntity, FromRootToEntity };
    RootMotionMode RootMotion = RootMotionMode::None;
    glm::vec3 _PrevRootModelPos{0.0f};
    bool _PrevRootValid = false;

    // Bimodal animator behavior
    enum class Mode { ControllerAnimated, AnimationPlayerAnimated };
    Mode AnimatorMode = Mode::AnimationPlayerAnimated;

    // Single-clip (Animation Player) mode configuration
    std::string SingleClipPath;      // Path to a unified .anim (preferred) or legacy clip; resolved to CachedAssets[0]
    bool PlayOnStart = true;         // If true, auto-begin playback on start
    bool IsPlaying = false;          // Runtime playing flag for single-clip mode
    bool _InitApplied = false;       // Internal guard to apply PlayOnStart once

    // Exposed runtime info for UI and scripting
    std::string Debug_CurrentAnimationName;      // clip/asset name currently bound
    std::string Debug_CurrentControllerStateName; // controller state name when in controller mode
};

} // namespace animation
} // namespace cm
