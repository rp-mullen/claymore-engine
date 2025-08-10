#include "animation/HumanoidRetargeter.h"
#include "ecs/AnimationComponents.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

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
}

} // namespace animation
} // namespace cm


