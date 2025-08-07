#pragma once

#include "ecs/Scene.h"
#include "animation/AnimationEvaluator.h"
#include "animation/AnimationPlayerComponent.h"
#include "ecs/AnimationComponents.h"

namespace cm {
namespace animation {

class AnimationSystem {
public:
    // Call each frame.
    static void Update(::Scene& scene, float deltaTime);
};

} // namespace animation
} // namespace cm
