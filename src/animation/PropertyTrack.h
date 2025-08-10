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

// Script event fired at a given time; will attempt to find a managed script
// with the given class name on the target entity and invoke the method.
struct ScriptEventKey {
    float Time = 0.0f;
    std::string ScriptClass;
    std::string Method;
};

struct ScriptEventTrack {
    std::string Name = "Script Events"; // single lane name
    std::vector<ScriptEventKey> Keys;
};

struct TimelineClip {
    std::string Name;
    float Length = 0.0f;                     // Seconds
    std::vector<PropertyTrack> Tracks;
    std::vector<ScriptEventTrack> ScriptTracks;
    // Optional skeletal clip references to play alongside property/script tracks
    struct SkeletalClipRef {
        std::string ClipPath;   // path to .anim
        float Speed = 1.0f;
        bool Loop = true;
    };
    std::vector<SkeletalClipRef> SkeletalClips;
};

} // namespace animation
} // namespace cm
