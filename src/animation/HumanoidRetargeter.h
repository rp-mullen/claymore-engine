#pragma once

#include <vector>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include "animation/AvatarDefinition.h"

struct SkeletonComponent;

namespace cm {
namespace animation {

struct RetargetSettings {
    bool PreserveTargetBoneLengths = true;
    bool ApplyRootXZ = true;
    bool ApplyRootY = false;
    bool ApplyRootYaw = true;
    bool MirrorIfNeeded = false;
};

class HumanoidRetargeter {
public:
    HumanoidRetargeter() = default;

    void Precompute(const AvatarDefinition& source, const AvatarDefinition& target);

    // srcLocalPose: local transforms for the source skeleton indexed by source bone indices.
    // outTargetLocalPose: local transforms for the target skeleton indexed by target bone indices.
    void RetargetPose(const SkeletonComponent& srcSkel,
                      const std::vector<glm::mat4>& srcLocalPose,
                      const SkeletonComponent& dstSkel,
                      std::vector<glm::mat4>& outTargetLocalPose,
                      const RetargetSettings& settings) const;

    void SetAvatars(const AvatarDefinition* src, const AvatarDefinition* dst);

private:
    const AvatarDefinition* m_Source = nullptr;
    const AvatarDefinition* m_Target = nullptr;
};

} // namespace animation
} // namespace cm


