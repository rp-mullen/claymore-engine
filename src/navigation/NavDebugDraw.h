#pragma once

#include <cstdint>
#include "navigation/NavTypes.h"

namespace nav { class NavMeshRuntime; struct NavMeshComponent; struct NavAgentComponent; }

namespace nav::debug
{
    void SetMask(NavDrawMask mask);
    NavDrawMask GetMask();

    void DrawRuntime(const NavMeshRuntime& rt, uint16_t viewId = 0);
    void DrawPath(const NavPath& path, uint16_t viewId = 0);
    void DrawAgent(const NavAgentComponent& agent, const glm::vec3& pos, const glm::vec3& vel, uint16_t viewId = 0);
}


