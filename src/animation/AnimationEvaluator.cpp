#include "animation/AnimationEvaluator.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <algorithm>
#include <unordered_map>

namespace cm {
namespace animation {

template<typename KeyContainer>
static size_t FindKeyframeIndex(const KeyContainer& keys, float time, size_t startIdx) {
    // Fast-forward cached index until the next key's time is > time
    while (startIdx + 1 < keys.size() && keys[startIdx + 1].Time < time) {
        ++startIdx;
    }
    return startIdx;
}

glm::vec3 SampleVec3(const std::vector<KeyframeVec3>& keys, float time, size_t& cacheIdx) {
    if (keys.empty()) return glm::vec3(0.0f);
    if (keys.size() == 1) return keys.front().Value;

    cacheIdx = FindKeyframeIndex(keys, time, cacheIdx);

    const auto& k0 = keys[cacheIdx];
    if (cacheIdx + 1 == keys.size()) return k0.Value;

    const auto& k1 = keys[cacheIdx + 1];
    float t = (time - k0.Time) / (k1.Time - k0.Time);
    return glm::mix(k0.Value, k1.Value, t);
}

glm::quat SampleQuat(const std::vector<KeyframeQuat>& keys, float time, size_t& cacheIdx) {
    if (keys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (keys.size() == 1) return keys.front().Value;

    cacheIdx = FindKeyframeIndex(keys, time, cacheIdx);

    const auto& k0 = keys[cacheIdx];
    if (cacheIdx + 1 == keys.size()) return k0.Value;

    const auto& k1 = keys[cacheIdx + 1];
    float t = (time - k0.Time) / (k1.Time - k0.Time);
    return glm::slerp(k0.Value, k1.Value, t);
}

void EvaluateAnimation(const AnimationClip& clip,
                       float time,
                       const ::SkeletonComponent& skeleton,
                       std::vector<glm::mat4>& outLocalTransforms,
                       const HumanoidAvatar* avatar) {
    // Ensure output container is sized correctly.
    outLocalTransforms.resize(skeleton.BoneEntities.size(), glm::mat4(1.0f));

    // Cache for each bone track – storing last index to speed up sampling.
    struct CacheEntry { size_t pos = 0; size_t rot = 0; size_t scale = 0; };
    std::unordered_map<std::string, CacheEntry> cache;

    // For every animated bone track in the clip
    for (const auto& [boneName, track] : clip.BoneTracks) {
        const std::string* resolvedName = &boneName;
        std::string tmp;
        if (avatar) {
            // Need to map through avatar – brute force search.
            for (const auto& [humanBone, mappedName] : avatar->BoneMapping) {
                if (mappedName == boneName) {
                    auto maybeDest = avatar->GetBoneName(humanBone);
                    if (maybeDest) {
                        tmp = *maybeDest;
                        resolvedName = &tmp;
                    }
                    break;
                }
            }
        }

        // Find bone index in skeleton via name mapping
        int idx = skeleton.GetBoneIndex(*resolvedName);
        if (idx < 0 || static_cast<size_t>(idx) >= skeleton.BoneEntities.size()) {
            continue; // Not found in this skeleton
        }
        size_t boneIdx = static_cast<size_t>(idx);

        CacheEntry& ce = cache[*resolvedName];

        glm::vec3 pos = track.PositionKeys.empty() ? glm::vec3(0.0f) : SampleVec3(track.PositionKeys, time, ce.pos);
        glm::quat rot = track.RotationKeys.empty() ? glm::quat(1,0,0,0) : SampleQuat(track.RotationKeys, time, ce.rot);
        glm::vec3 scl = track.ScaleKeys.empty() ? glm::vec3(1.0f) : SampleVec3(track.ScaleKeys, time, ce.scale);

        glm::mat4 mat = glm::translate(pos) * glm::mat4_cast(rot) * glm::scale(scl);
        outLocalTransforms[boneIdx] = mat;
    }
}

} // namespace animation
} // namespace cm
