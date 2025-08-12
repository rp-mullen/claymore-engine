// AnimationPreviewPlayer.cpp
#include "animation/AnimationPreviewPlayer.h"
#include "animation/AnimationTypes.h"
#include "animation/AnimationEvaluator.h"
#include "animation/AnimationAsset.h"
#include "animation/BindingCache.h"
#include "animation/PreviewContext.h"
#include "animation/HumanoidRetargeter.h"
#include "ecs/AnimationComponents.h"
#include "ecs/Scene.h"

using cm::animation::AnimationClip;

AnimationPreviewPlayer::AnimationPreviewPlayer() = default;
AnimationPreviewPlayer::~AnimationPreviewPlayer() = default;

void AnimationPreviewPlayer::SetClip(std::shared_ptr<AnimationClip> clip) { m_Clip = std::move(clip); m_Time = 0.0f; }
void AnimationPreviewPlayer::SetAsset(const cm::animation::AnimationAsset* asset) { m_Asset = asset; m_Time = 0.0f; }
void AnimationPreviewPlayer::SetSkeleton(SkeletonComponent* skel) { m_Skeleton = skel; }
void AnimationPreviewPlayer::SetAvatar(const cm::animation::AvatarDefinition* avatar, const SkeletonComponent* skeleton) { m_Humanoid = avatar; m_Skeleton = skeleton; }
void AnimationPreviewPlayer::SetLoop(bool loop) { m_Loop = loop; }
void AnimationPreviewPlayer::SetSpeed(float s) { m_Speed = s; }
void AnimationPreviewPlayer::SetRetargetMap(const cm::animation::AvatarDefinition* map) { m_Retarget = map; }
void AnimationPreviewPlayer::SetScene(Scene* scene) { m_Scene = scene; }
void AnimationPreviewPlayer::SetTime(float t) { m_Time = t; }
float AnimationPreviewPlayer::GetTime() const { return m_Time; }
float AnimationPreviewPlayer::GetDuration() const { return m_Clip ? m_Clip->Duration : 0.0f; }

void AnimationPreviewPlayer::Update(float dt)
{
    m_Time += dt * m_Speed;
    // Legacy skeletal clip preview (writes only to PreviewScene)
    if (m_Clip && m_Skeleton && m_Scene) {
        if (m_Loop && m_Clip->Duration > 0.0f) {
            while (m_Time > m_Clip->Duration) m_Time -= m_Clip->Duration;
            if (m_Time < 0.0f) m_Time += m_Clip->Duration;
        } else {
            if (m_Time > m_Clip->Duration) m_Time = m_Clip->Duration;
            if (m_Time < 0.0f) m_Time = 0.0f;
        }
        std::vector<glm::mat4> localPose;
        cm::animation::EvaluateAnimation(*m_Clip, m_Time, *m_Skeleton, localPose, nullptr);
        for (size_t i = 0; i < localPose.size() && i < m_Skeleton->BoneEntities.size(); ++i) {
            EntityID boneId = m_Skeleton->BoneEntities[i];
            if (boneId == (EntityID)-1) continue;
            if (auto* data = m_Scene->GetEntityData(boneId)) {
                data->Transform.LocalMatrix = localPose[i];
                data->Transform.TransformDirty = true;
            }
        }
        return;
    }

    // Unified asset preview path (writes only to PreviewScene)
    if (m_Asset && m_Skeleton && m_Scene) {
        float len = m_Asset->Duration();
        if (m_Loop && len > 0.0f) {
            while (m_Time > len) m_Time -= len;
            if (m_Time < 0.0f) m_Time += len;
        } else {
            if (m_Time > len) m_Time = len;
            if (m_Time < 0.0f) m_Time = 0.0f;
        }
        if (!m_Bindings) m_Bindings = std::make_unique<cm::animation::BindingCache>();
        m_Bindings->SetSkeleton(m_Skeleton);
        cm::animation::PoseBuffer pose; pose.local.resize(m_Skeleton->BoneEntities.size(), glm::mat4(1.0f)); pose.touched.resize(m_Skeleton->BoneEntities.size(), false);
        cm::animation::EvalInputs in{ m_Asset, m_Time, m_Loop };
        cm::animation::EvalTargets tgt{ &pose };
        cm::animation::EvalContext ctx{ m_Bindings.get(), m_Humanoid, m_Skeleton };
        std::vector<cm::animation::ScriptEvent> events; nlohmann::json props;
        cm::animation::SampleAsset(in, ctx, tgt, &events, &props);
        for (size_t i = 0; i < pose.local.size() && i < m_Skeleton->BoneEntities.size(); ++i) {
            EntityID boneId = m_Skeleton->BoneEntities[i];
            if (boneId == (EntityID)-1) continue;
            if (auto* data = m_Scene->GetEntityData(boneId)) {
                data->Transform.LocalMatrix = pose.local[i];
                data->Transform.TransformDirty = true;
            }
        }
    }
}

void AnimationPreviewPlayer::SampleTo(cm::animation::PreviewContext& ctx)
{
    if (!m_Asset || !m_Skeleton) return;
    if (!m_Bindings) m_Bindings = std::make_unique<cm::animation::BindingCache>();
    m_Bindings->SetSkeleton(m_Skeleton);

    cm::animation::EvalInputs in{ m_Asset, m_Time, m_Loop };
    cm::animation::EvalTargets out{ &ctx.pose };
    cm::animation::EvalContext cxt{ m_Bindings.get(), m_Humanoid, m_Skeleton };
    std::vector<cm::animation::ScriptEvent> events;
    nlohmann::json propWrites;
    cm::animation::SampleAsset(in, cxt, out, &events, &propWrites);
}


