#include "animation/Retargeting.h"

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

} // namespace animation
} // namespace cm
