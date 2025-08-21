#include "navigation/NavDebugDraw.h"
#include "navigation/NavMesh.h"
#include "rendering/Renderer.h"

using namespace nav;

static std::atomic<uint32_t> sMask{ (uint32_t)NavDrawMask::None };

void nav::debug::SetMask(NavDrawMask mask) { sMask.store((uint32_t)mask); }
NavDrawMask nav::debug::GetMask() { return (NavDrawMask)sMask.load(); }

void nav::debug::DrawRuntime(const NavMeshRuntime& rt, uint16_t viewId)
{
    const uint32_t mask = sMask.load();
    if (!(mask & (uint32_t)NavDrawMask::TriMesh) && !(mask & (uint32_t)NavDrawMask::Polys)) return;

    // Wireframe triangles
    if (mask & (uint32_t)NavDrawMask::TriMesh) {
        for (const auto& p : rt.m_Polys) {
            const glm::vec3 a = rt.m_Vertices[p.i0];
            const glm::vec3 b = rt.m_Vertices[p.i1];
            const glm::vec3 c = rt.m_Vertices[p.i2];
            Renderer::Get().DrawDebugRay(a, b - a, 1.0f);
            Renderer::Get().DrawDebugRay(b, c - b, 1.0f);
            Renderer::Get().DrawDebugRay(c, a - c, 1.0f);
        }
    }
}

void nav::debug::DrawPath(const NavPath& path, uint16_t viewId)
{
    if (!(sMask.load() & (uint32_t)NavDrawMask::Path)) return;
    for (size_t i = 1; i < path.points.size(); ++i) {
        glm::vec3 a = path.points[i-1]; glm::vec3 b = path.points[i];
        Renderer::Get().DrawDebugRay(a, b - a, 1.0f);
    }
}

void nav::debug::DrawAgent(const NavAgentComponent& agent, const glm::vec3& pos, const glm::vec3& vel, uint16_t viewId)
{
    if (!(sMask.load() & (uint32_t)NavDrawMask::Agents)) return;
    // Draw velocity vector
    Renderer::Get().DrawDebugRay(pos, vel, 1.0f);
}


