#include "animation/Retargeting.h"
#include "ecs/AnimationComponents.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include "animation/AnimationEvaluator.h" // for PoseBuffer definition

namespace cm {
namespace animation {

AnimationClip RetargetAnimation(const AnimationClip& srcClip,
                                const HumanoidAvatar& srcAvatar,
                                const HumanoidAvatar& dstAvatar) {
    AnimationClip result;
    result.Name            = srcClip.Name + "_retargeted";
    result.Duration        = srcClip.Duration;
    result.TicksPerSecond  = srcClip.TicksPerSecond;

    // For every human bone in the source avatar, copy its track over to the
    // corresponding bone in the destination avatar (if it exists).
    for (const auto& [humanBone, srcBoneName] : srcAvatar.BoneMapping) {
        // Find track in source clip
        auto trackIt = srcClip.BoneTracks.find(srcBoneName);
        if (trackIt == srcClip.BoneTracks.end()) continue; // Bone not animated in source

        // Find destination bone name
        auto dstIt = dstAvatar.BoneMapping.find(humanBone);
        if (dstIt == dstAvatar.BoneMapping.end()) continue; // Destination skeleton doesn't have equivalent bone

        const std::string& dstBoneName = dstIt->second;
        result.BoneTracks.emplace(dstBoneName, trackIt->second);
    }

    return result;
}

void RetargetAvatarToSkeleton(const AssetAvatarTrack& track,
                              const HumanoidAvatar& avatar,
                              const ::SkeletonComponent& skeleton,
                              PoseBuffer& outPose,
                              float time,
                              bool loop,
                              float length)
{
    int boneIndex = avatar.HumanToSkeleton(track.humanBoneId, skeleton);
    if (boneIndex < 0) return;
    glm::vec3 pos = track.t.keys.empty() ? glm::vec3(0.0f) : track.t.Sample(time, loop, length);
    glm::quat rot = track.r.keys.empty() ? glm::quat(1,0,0,0) : track.r.Sample(time, loop, length);
    glm::vec3 scl = track.s.keys.empty() ? glm::vec3(1.0f) : track.s.Sample(time, loop, length);
    if (static_cast<size_t>(boneIndex) >= outPose.local.size()) outPose.local.resize(boneIndex + 1, glm::mat4(1.0f));
    if (static_cast<size_t>(boneIndex) >= outPose.touched.size()) outPose.touched.resize(boneIndex + 1, false);
    outPose.local[static_cast<size_t>(boneIndex)] = glm::translate(pos) * glm::mat4_cast(rot) * glm::scale(scl);
    outPose.touched[static_cast<size_t>(boneIndex)] = true;
}

} // namespace animation
} // namespace cm
