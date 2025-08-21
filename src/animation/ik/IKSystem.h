// IKSystem.h
#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "animation/ik/IKComponent.h"

class Scene;
struct SkeletonComponent;

namespace cm { namespace animation { namespace ik {

class IKSystem {
public:
    static IKSystem& Get() { static IKSystem s; return s; }

    // Run after animation sampling, before transforms/skinning
    void SolveAndBlend(Scene& scene, float deltaTime);

private:
    IKSystem() = default;
};

} } }


