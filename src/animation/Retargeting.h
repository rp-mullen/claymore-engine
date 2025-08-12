#pragma once

#include "animation/AnimationTypes.h"
#include "animation/HumanoidAvatar.h"
#include "animation/AnimationAsset.h"

namespace cm {
namespace animation {

struct PoseBuffer; // forward
// Retarget srcClip which was authored for srcAvatar, onto dstAvatar.
// Returns a new retargeted clip.
AnimationClip RetargetAnimation(const AnimationClip& srcClip,
                                const HumanoidAvatar& srcAvatar,
                                const HumanoidAvatar& dstAvatar);

// New helper for unified AvatarTrack retarget to skeleton pose buffer
void RetargetAvatarToSkeleton(const AssetAvatarTrack& track,
                              const HumanoidAvatar& avatar,
                              const ::SkeletonComponent& skeleton,
                              PoseBuffer& outPose,
                              float time,
                              bool loop,
                              float length);

} // namespace animation
} // namespace cm
