#include "animation/HumanoidRetargeter.h"
#include "ecs/AnimationComponents.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/quaternion.hpp>

namespace cm {
namespace animation {

void HumanoidRetargeter::SetAvatars(const AvatarDefinition* src, const AvatarDefinition* dst)
{
    m_Source = src;
    m_Target = dst;
}

void HumanoidRetargeter::Precompute(const AvatarDefinition& source, const AvatarDefinition& target)
{
    m_Source = &source;
    m_Target = &target;

    // Precompute model-space retarget matrices R[b] = T_bind * inverse(S_bind)
    for (uint16_t i = 0; i < HumanoidBoneCount; ++i) {
        if (!source.Present[i] || !target.Present[i]) continue;
        glm::mat4 sBind = source.BindModel[i];
        glm::mat4 tBind = target.BindModel[i];
        // Store in target avatar for convenience
        // Note: We put it in target.RetargetModel for quick access
        // But since target is const, assume caller prepared target.RetargetModel externally when saving asset.
        // Here we just rely on m_Source/m_Target bind matrices.
        (void)sBind; (void)tBind; // no-op; computation done inline in RetargetPose.
    }
}

static glm::mat4 compose_trs(const glm::vec3& t, const glm::quat& r, const glm::vec3& s)
{
    return glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
}

// --- Twist utilities ---
static inline void DecomposeSwingTwist(const glm::quat& q,
                                       const glm::vec3& axis,
                                       glm::quat& outSwing,
                                       glm::quat& outTwist)
{
    glm::vec3 qv(q.x, q.y, q.z);
    glm::vec3 proj = axis * glm::dot(qv, axis);
    outTwist = glm::normalize(glm::quat(q.w, proj.x, proj.y, proj.z));
    outSwing = q * glm::conjugate(outTwist);
}

static inline glm::quat QuatPow(const glm::quat& q, float t)
{
    glm::quat nq = glm::normalize(q);
    float w = glm::clamp(nq.w, -1.0f, 1.0f);
    float angle = std::acos(w);
    if (angle < 1e-6f) return nq;
    float newAngle = angle * t;
    float s = std::sin(newAngle) / std::sin(angle);
    return glm::normalize(glm::quat(std::cos(newAngle), nq.x * s, nq.y * s, nq.z * s));
}

void HumanoidRetargeter::RetargetPose(const SkeletonComponent& srcSkel,
                                      const std::vector<glm::mat4>& srcLocalPose,
                                      const SkeletonComponent& dstSkel,
                                      std::vector<glm::mat4>& outTargetLocalPose,
                                      const RetargetSettings& settings) const
{
    if (!m_Source || !m_Target) {
        outTargetLocalPose.clear();
        return;
    }

    const auto countDst = dstSkel.BoneEntities.size();
    outTargetLocalPose.assign(countDst, glm::mat4(1.0f));

    // For each humanoid bone present in both, transfer local delta
    for (uint16_t i = 0; i < HumanoidBoneCount; ++i) {
        if (!m_Source->Present[i] || !m_Target->Present[i]) continue;
        const int32_t sIdx = m_Source->Map[i].BoneIndex;
        const int32_t tIdx = m_Target->Map[i].BoneIndex;
        if (sIdx < 0 || tIdx < 0) continue;

        // Source local bind and animated local
        glm::mat4 sBindLocal = m_Source->BindLocal[i];
        glm::mat4 sAnimLocal = (size_t)sIdx < srcLocalPose.size() ? srcLocalPose[sIdx] : glm::mat4(1.0f);
        glm::mat4 srcDeltaLocal = sAnimLocal * glm::inverse(sBindLocal);

        // Map delta into target local frame: D * T_bindLocal
        glm::mat4 tBindLocal = m_Target->BindLocal[i];
        glm::mat4 tAnimLocal = srcDeltaLocal * tBindLocal;

        outTargetLocalPose[(size_t)tIdx] = tAnimLocal;
    }

    // Optional: preserve target bone lengths by normalizing scale along child axes
    if (settings.PreserveTargetBoneLengths) {
        for (uint16_t i = 0; i < HumanoidBoneCount; ++i) {
            if (!m_Target->Present[i]) continue;
            const int32_t tIdx = m_Target->Map[i].BoneIndex;
            if (tIdx < 0 || (size_t)tIdx >= outTargetLocalPose.size()) continue;

            glm::vec3 t, skew, scl; glm::vec4 persp; glm::quat r;
            glm::decompose(outTargetLocalPose[(size_t)tIdx], scl, r, t, skew, persp);
            // Force unit scale; rely on target bind local to encode lengths
            outTargetLocalPose[(size_t)tIdx] = compose_trs(t, r, glm::vec3(1.0f));
        }
    }

    // --- Split twist for targets that have explicit twist joints ---
    auto splitTwist = [&](HumanoidBone baseBone, HumanoidBone twistBone, HumanoidBone childBone, float ratio)
    {
        if (!m_Target->IsBonePresent(baseBone) || !m_Target->IsBonePresent(twistBone) || !m_Target->IsBonePresent(childBone)) return;
        const int32_t baseIdx = m_Target->GetMappedBoneIndex(baseBone);
        const int32_t twistIdx = m_Target->GetMappedBoneIndex(twistBone);
        const int32_t childIdx = m_Target->GetMappedBoneIndex(childBone);
        if (baseIdx < 0 || twistIdx < 0 || childIdx < 0) return;
        if (baseIdx == twistIdx) return; // virtual twist mapped to base – nothing to distribute
        if ((size_t)baseIdx >= outTargetLocalPose.size() || (size_t)twistIdx >= outTargetLocalPose.size()) return;

        // Axis from base to child in bind pose (model space), normalized
        const glm::vec3 basePos = glm::vec3(m_Target->BindModel[(uint16_t)baseBone][3]);
        const glm::vec3 childPos = glm::vec3(m_Target->BindModel[(uint16_t)childBone][3]);
        glm::vec3 axis = childPos - basePos;
        float len = glm::length(axis);
        if (len < 1e-6f) return;
        axis /= len;

        // Decompose base local rotation and distribute twist between base and twist
        glm::vec3 tBase, skew, scl; glm::vec4 persp; glm::quat rBase;
        glm::decompose(outTargetLocalPose[(size_t)baseIdx], scl, rBase, tBase, skew, persp);
        glm::vec3 tTwist; glm::quat rTwist; // current twist pose translation kept
        glm::decompose(outTargetLocalPose[(size_t)twistIdx], scl, rTwist, tTwist, skew, persp);

        glm::quat swing, twistQ;
        DecomposeSwingTwist(rBase, axis, swing, twistQ);

        glm::quat newBaseR = glm::normalize(swing * QuatPow(twistQ, 1.0f - ratio));
        glm::quat newTwistR = glm::normalize(QuatPow(twistQ, ratio));

        outTargetLocalPose[(size_t)baseIdx]  = compose_trs(tBase,  newBaseR,  glm::vec3(1.0f));
        outTargetLocalPose[(size_t)twistIdx] = compose_trs(tTwist, newTwistR, glm::vec3(1.0f));
    };

    const float twistShare = 0.5f;
    splitTwist(HumanoidBone::LeftUpperArm,  HumanoidBone::LeftUpperArmTwist,  HumanoidBone::LeftLowerArm,  twistShare);
    splitTwist(HumanoidBone::LeftLowerArm,  HumanoidBone::LeftLowerArmTwist,  HumanoidBone::LeftHand,      twistShare);
    splitTwist(HumanoidBone::RightUpperArm, HumanoidBone::RightUpperArmTwist, HumanoidBone::RightLowerArm, twistShare);
    splitTwist(HumanoidBone::RightLowerArm, HumanoidBone::RightLowerArmTwist, HumanoidBone::RightHand,     twistShare);

    splitTwist(HumanoidBone::LeftUpperLeg,  HumanoidBone::LeftUpperLegTwist,  HumanoidBone::LeftLowerLeg,  twistShare);
    splitTwist(HumanoidBone::LeftLowerLeg,  HumanoidBone::LeftLowerLegTwist,  HumanoidBone::LeftFoot,      twistShare);
    splitTwist(HumanoidBone::RightUpperLeg, HumanoidBone::RightUpperLegTwist, HumanoidBone::RightLowerLeg, twistShare);
    splitTwist(HumanoidBone::RightLowerLeg, HumanoidBone::RightLowerLegTwist, HumanoidBone::RightFoot,     twistShare);
}

} // namespace animation
} // namespace cm


