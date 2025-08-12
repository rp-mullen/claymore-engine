// PreviewAvatarCache.h
#pragma once

#include <optional>
#include <string>
#include <tuple>

#include "rendering/ModelLoader.h"           // Model
#include "ecs/AnimationComponents.h"         // SkeletonComponent
#include "animation/AvatarDefinition.h"      // AvatarDefinition
#include "animation/AnimationTypes.h"        // AnimationClip

class PreviewAvatarCache {
public:
    // Returns (Model, Skeleton, HumanoidMap?)
    std::tuple<Model, SkeletonComponent, const cm::animation::AvatarDefinition*>
    ResolveForClip(const cm::animation::AnimationClip& clip);
};


