#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>
#include <shared_mutex>
#include <glm/glm.hpp>

namespace nav
{
    struct NavAreaId { uint16_t value = 0; };
    struct NavFlags { uint32_t mask = 0; };

    struct NavAgentParams {
        float radius = 0.4f;
        float height = 1.8f;
        float maxSlopeDeg = 45.0f;
        float maxStep = 0.4f;
        float maxSpeed = 3.0f;
        float maxAccel = 8.0f;
    };

    struct NavBakeSettings {
        float cellSize = 0.2f;
        float cellHeight = 0.2f;
        float agentRadius = 0.4f;
        float agentHeight = 1.8f;
        float agentMaxClimb = 0.4f;
        float agentMaxSlopeDeg = 45.0f;
        float regionMinSize = 2.0f;
        float regionMergeSize = 20.0f;
        float edgeMaxLen = 12.0f;
        float edgeMaxError = 1.3f;
        float vertsPerPoly = 6.0f;
        float detailSampleDist = 6.0f;
        float detailSampleMaxError = 1.0f;
        uint32_t seed = 0xC0FFEEu;
    };

    enum class NavDrawMask : uint32_t {
        None   = 0,
        TriMesh= 1u << 0,
        Polys  = 1u << 1,
        BVTree = 1u << 2,
        Path   = 1u << 3,
        Links  = 1u << 4,
        Agents = 1u << 5,
        All    = 0xFFFFFFFFu
    };

    inline NavDrawMask operator|(NavDrawMask a, NavDrawMask b) { return static_cast<NavDrawMask>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); }
    inline NavDrawMask operator&(NavDrawMask a, NavDrawMask b) { return static_cast<NavDrawMask>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); }

    struct NavPath {
        std::vector<glm::vec3> points;
        bool valid = false;
    };

    struct Bounds {
        glm::vec3 min{ 0.0f };
        glm::vec3 max{ 0.0f };

        void expand(const glm::vec3& p) {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }
        glm::vec3 center() const { return (min + max) * 0.5f; }
        glm::vec3 extents() const { return (max - min) * 0.5f; }
    };
}


