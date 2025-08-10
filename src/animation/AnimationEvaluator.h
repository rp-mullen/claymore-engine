#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "animation/AnimationTypes.h"
#include "animation/AvatarDefinition.h"
#include "ecs/AnimationComponents.h"

namespace cm {
namespace animation {

// Forward declarations


// Simple linear interpolation helpers for keyframes
glm::vec3 SampleVec3(const std::vector<KeyframeVec3>& keys, float time, size_t& cacheIdx);
glm::quat SampleQuat(const std::vector<KeyframeQuat>& keys, float time, size_t& cacheIdx);

// Evaluate a skeletal clip at given time. The resulting local transforms (one per
// skeleton bone) are written to outLocalTransforms.
// If an avatar is supplied for retargeting, use it to map source clip bone tracks to destination skeleton
// using HumanoidBone mapping. This is a minimal name-to-name indirection; full retargeting happens in HumanoidRetargeter.
void EvaluateAnimation(const AnimationClip& clip,
                       float time,
                       const ::SkeletonComponent& skeleton,
                       std::vector<glm::mat4>& outLocalTransforms,
                       const AvatarDefinition* avatar = nullptr);

} // namespace animation
} // namespace cm
