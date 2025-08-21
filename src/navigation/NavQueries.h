#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <queue>
#include <limits>
#include "navigation/NavTypes.h"

namespace nav { class NavMeshRuntime; }

namespace nav::queries
{
    // A* over triangle centers with Euclidean heuristic, then funnel smoothing
    bool FindPath(const NavMeshRuntime& nm, const glm::vec3& start, const glm::vec3& end,
                  const NavAgentParams& params, NavFlags include, NavFlags exclude, NavPath& out);

    bool RaycastPolyMesh(const NavMeshRuntime& nm, const glm::vec3& start, const glm::vec3& end,
                         float& tHit, glm::vec3& hitNormal);

    bool NearestPointOnNavmesh(const NavMeshRuntime& nm, const glm::vec3& pos, float maxDist,
                               glm::vec3& outOnMesh);

    bool RandomPointInRadius(const NavMeshRuntime& nm, const glm::vec3& pos, float r,
                             glm::vec3& out);
}


