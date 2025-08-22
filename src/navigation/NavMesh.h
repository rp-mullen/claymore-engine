#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <array>
#include <shared_mutex>
#include <atomic>
#include <glm/glm.hpp>
#include "navigation/NavTypes.h"

#include "pipeline/AssetReference.h"

class Scene;
class Mesh;

using EntityID = uint32_t;

namespace nav
{
    class NavMeshRuntime;

    struct OffMeshLink
    {
        glm::vec3 a{0};
        glm::vec3 b{0};
        float radius = 0.5f;
        uint32_t flags = 0;
        uint8_t bidir = 1; // 1 = bidirectional
    };

    // ECS component: authoring and baked data reference
    struct NavMeshComponent
    {
        bool Enabled = true;
        NavBakeSettings Bake;
        std::vector<EntityID> SourceMeshes;
        AssetReference BakedAsset; // .navbin
        Bounds AABB;
        uint64_t BakeHash = 0;
        std::shared_ptr<NavMeshRuntime> Runtime;

        // baking state (async)
        std::atomic<bool> Baking{false};
        std::atomic<float> BakingProgress{0.0f};
        std::atomic<bool> BakingCancel{false};

        uint64_t ComputeBakeHash(Scene& scene) const;
        // If no explicit SourceMeshes are set, returns owning entity plus all descendants with meshes
        void GetEffectiveSources(Scene& scene, std::vector<EntityID>& out) const;
        void RequestBake(Scene& scene);
        void CancelBake();
        bool IsBaking() const { return Baking.load(); }
        float BakeProgress() const { return BakingProgress.load(); }
        bool EnsureRuntimeLoaded();
    };

    // Runtime built from navbin
    class NavMeshRuntime
    {
    public:
        // Polygon is just a triangle for now (indices into m_Vertices)
        struct Poly { uint32_t i0, i1, i2; uint16_t area; uint32_t flags; };

        // Adjacency by poly index -> neighboring polys that share an edge
        std::vector<std::vector<uint32_t>> m_Adjacency;

        // Geometry
        std::vector<glm::vec3> m_Vertices;
        std::vector<Poly> m_Polys;
        std::vector<OffMeshLink> m_Links;

        // Accel structures
        struct BVNode { Bounds b; uint32_t left = UINT32_MAX, right = UINT32_MAX, start = 0, count = 0; };
        std::vector<BVNode> m_BVH;
        std::vector<uint32_t> m_BVHIndices; // triangle indices per leaf

        Bounds m_Bounds;

        // Area costs (per NavAreaId.value)
        std::array<float, 64> m_AreaCost{}; // default 1.0f

        mutable std::shared_mutex m_Lock;

        NavMeshRuntime();

        bool FindPath(const glm::vec3& start, const glm::vec3& end, NavPath& out, const NavAgentParams& params, NavFlags include, NavFlags exclude) const;
        bool Raycast(const glm::vec3& start, const glm::vec3& end, float& tHit, glm::vec3& hitNormal) const;
        bool NearestPoint(const glm::vec3& pos, float maxDist, glm::vec3& outOnMesh) const;

        void RebuildBVH();
    };
}


