// AnimationPreviewPlayer.h
#pragma once

#include <cstddef>
#include <memory>

namespace cm { namespace animation { struct AnimationClip; class HumanoidRetargeter; struct AvatarDefinition; struct AnimationAsset; class BindingCache; class PreviewContext; } }
struct SkeletonComponent;
class Scene;

class AnimationPreviewPlayer {
public:
    AnimationPreviewPlayer();
    ~AnimationPreviewPlayer();

    // Legacy clip setter kept for compatibility in AnimationInspector; will be removed after migration
    void SetClip(std::shared_ptr<cm::animation::AnimationClip> clip);

    // New unified asset + avatar entry points
    void SetAsset(const cm::animation::AnimationAsset* asset);
    void SetAvatar(const cm::animation::AvatarDefinition* avatar, const SkeletonComponent* skeleton);
    void SetSkeleton(SkeletonComponent* skel); // declaration to match .cpp
    void SetLoop(bool loop);
    void SetSpeed(float s);
    void SetRetargetMap(const cm::animation::AvatarDefinition* map);
    void SetScene(Scene* scene);

    void SetTime(float t);
    float GetTime() const;
    float GetDuration() const;

    void Update(float dt);
    void SampleTo(cm::animation::PreviewContext& ctx);

private:
    std::shared_ptr<cm::animation::AnimationClip> m_Clip;
    const cm::animation::AnimationAsset* m_Asset = nullptr;
    const SkeletonComponent* m_Skeleton = nullptr;
    const cm::animation::AvatarDefinition* m_Humanoid = nullptr;
    const cm::animation::AvatarDefinition* m_Retarget = nullptr;
    std::unique_ptr<cm::animation::BindingCache> m_Bindings;
    Scene* m_Scene = nullptr;
    bool m_Loop = true;
    float m_Speed = 1.0f;
    float m_Time = 0.0f;
};


