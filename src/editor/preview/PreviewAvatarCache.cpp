// PreviewAvatarCache.cpp
#include "editor/preview/PreviewAvatarCache.h"
#include "rendering/ModelLoader.h"
#include "animation/AvatarSerializer.h"
#include "animation/AnimationTypes.h"
#include "ecs/AnimationComponents.h"
#include <tuple>

std::tuple<Model, SkeletonComponent, const cm::animation::AvatarDefinition*>
PreviewAvatarCache::ResolveForClip(const cm::animation::AnimationClip& clip)
{
    Model model{};
    SkeletonComponent skel{};
    static cm::animation::AvatarDefinition defaultAvatar{};
    const cm::animation::AvatarDefinition* avatar = nullptr;

    if (!clip.SourceAvatarPath.empty()) {
        cm::animation::AvatarDefinition tmp;
        if (cm::animation::LoadAvatar(tmp, clip.SourceAvatarPath)) {
            defaultAvatar = tmp;
            avatar = &defaultAvatar;
        }
    }
    // Fallback: load a simple mannequin if available in project assets (omitted here)
    return std::make_tuple(std::move(model), std::move(skel), avatar);
}


