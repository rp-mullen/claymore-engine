#pragma once

#include "animation/AnimationTypes.h"
#include "animation/HumanoidAvatar.h"

namespace cm {
namespace animation {

// Retarget srcClip which was authored for srcAvatar, onto dstAvatar.
// Returns a new retargeted clip.
AnimationClip RetargetAnimation(const AnimationClip& srcClip,
                                const HumanoidAvatar& srcAvatar,
                                const HumanoidAvatar& dstAvatar);

} // namespace animation
} // namespace cm
