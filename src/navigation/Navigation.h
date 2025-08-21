#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include "navigation/NavTypes.h"

class Scene;

namespace nav
{
    struct NavAgentComponent;
    struct NavMeshComponent;
    class NavMeshRuntime;

    class Navigation
    {
    public:
        static Navigation& Get();

        void Update(Scene& scene, float dt);

        // Global debug mask
        void SetDebugMask(NavDrawMask mask);
        NavDrawMask GetDebugMask() const;

        // C++ API wrappers
        bool FindPath(Scene& scene, uint32_t navMeshEntity, const glm::vec3& start, const glm::vec3& end,
                      const NavAgentParams& p, NavFlags include, NavFlags exclude, NavPath& out);
        bool Raycast(Scene& scene, uint32_t navMeshEntity, const glm::vec3& start, const glm::vec3& end, float& tHit, glm::vec3& hitNormal);
        bool NearestPoint(Scene& scene, uint32_t navMeshEntity, const glm::vec3& pos, float maxDist, glm::vec3& outOnMesh);

    private:
        Navigation() = default;
        NavDrawMask m_DebugMask = NavDrawMask::None;
    };
}


