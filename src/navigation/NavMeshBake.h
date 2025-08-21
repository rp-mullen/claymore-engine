#pragma once

#include <vector>
#include <memory>
#include "navigation/NavTypes.h"

namespace nav { class NavMeshRuntime; struct NavMeshComponent; }
class Scene;

namespace nav::bake
{
    struct NavMeshBinary
    {
        std::vector<glm::vec3> vertices;
        std::vector<uint32_t> indices;
        Bounds bounds;
    };

    bool BuildFromScene(Scene& scene, const NavMeshComponent& comp, NavMeshBinary& out);
    bool BuildRuntime(const NavMeshBinary& bin, std::shared_ptr<NavMeshRuntime>& out);
}


