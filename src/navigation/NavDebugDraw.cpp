#include "navigation/NavDebugDraw.h"
#include "navigation/NavMesh.h"
#include "rendering/Renderer.h"
#include "rendering/ShaderManager.h"
#include "rendering/VertexTypes.h"

using namespace nav;

static std::atomic<uint32_t> sMask{ (uint32_t)NavDrawMask::None };

void nav::debug::SetMask(NavDrawMask mask) { sMask.store((uint32_t)mask); }
NavDrawMask nav::debug::GetMask() { return (NavDrawMask)sMask.load(); }

void nav::debug::DrawRuntime(const NavMeshRuntime& rt, uint16_t viewId)
{
    const uint32_t mask = sMask.load();
    if (!(mask & (uint32_t)NavDrawMask::TriMesh) && !(mask & (uint32_t)NavDrawMask::Polys)) return;

    // Lazy-load a simple position+color shader for solid and line overlays
    static bgfx::ProgramHandle sColorProgram = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(sColorProgram)) {
        // Ensure vertex layout initialized
        PosColorVertex::Init();
        // Use custom passthrough VS/FS that handle vertex color
        sColorProgram = ShaderManager::Instance().LoadProgram("vs_navcolor", "fs_navcolor");
    }
    if (!bgfx::isValid(sColorProgram)) return; // bail if program failed

    // Build per-vertex normals for a small offset to avoid z-fighting
    std::vector<glm::vec3> normals(rt.m_Vertices.size(), glm::vec3(0.0f));
    for (const auto& p : rt.m_Polys) {
        const glm::vec3& a = rt.m_Vertices[p.i0];
        const glm::vec3& b = rt.m_Vertices[p.i1];
        const glm::vec3& c = rt.m_Vertices[p.i2];
        glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
        if (glm::any(glm::isnan(n))) n = glm::vec3(0,1,0);
        normals[p.i0] += n; normals[p.i1] += n; normals[p.i2] += n;
    }
    for (auto& n : normals) { float len = glm::length(n); n = (len > 1e-6f) ? (n / len) : glm::vec3(0,1,0); }

    // Helper to pack RGBA to ABGR as used by PosColorVertex
    auto packABGR = [](float r, float g, float b, float a) -> uint32_t {
        uint8_t R = (uint8_t)(glm::clamp(r, 0.0f, 1.0f) * 255.0f);
        uint8_t G = (uint8_t)(glm::clamp(g, 0.0f, 1.0f) * 255.0f);
        uint8_t B = (uint8_t)(glm::clamp(b, 0.0f, 1.0f) * 255.0f);
        uint8_t A = (uint8_t)(glm::clamp(a, 0.0f, 1.0f) * 255.0f);
        return (uint32_t(A) << 24) | (uint32_t(B) << 16) | (uint32_t(G) << 8) | uint32_t(R);
    };

    // Submit filled mesh (semi-transparent blue)
    if (mask & (uint32_t)NavDrawMask::Polys) {
        const float offset = 0.01f;
        std::vector<PosColorVertex> verts; verts.resize(rt.m_Vertices.size());
        for (size_t i = 0; i < rt.m_Vertices.size(); ++i) {
            glm::vec3 p = rt.m_Vertices[i] + normals[i] * offset;
            verts[i].x = p.x; verts[i].y = p.y; verts[i].z = p.z;
            verts[i].abgr = packABGR(0.1f, 0.4f, 1.0f, 0.25f);
        }
        std::vector<uint32_t> idx; idx.reserve(rt.m_Polys.size() * 3);
        for (const auto& p : rt.m_Polys) { idx.push_back(p.i0); idx.push_back(p.i1); idx.push_back(p.i2); }

        if (!verts.empty() && !idx.empty()) {
            const bgfx::Memory* vmem = bgfx::copy(verts.data(), (uint32_t)(verts.size() * sizeof(PosColorVertex)));
            const bgfx::Memory* imem = bgfx::copy(idx.data(), (uint32_t)(idx.size() * sizeof(uint32_t)));
            bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(vmem, PosColorVertex::layout);
            bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(imem, BGFX_BUFFER_INDEX32);
            float identity[16]; bx::mtxIdentity(identity); bgfx::setTransform(identity);
            bgfx::setVertexBuffer(0, vbh);
            bgfx::setIndexBuffer(ibh);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_BLEND_ALPHA);
            bgfx::submit(viewId, sColorProgram);
            bgfx::destroy(vbh);
            bgfx::destroy(ibh);
        }
    }

    // Wireframe overlay (opaque brighter blue), also offset a bit
    if (mask & (uint32_t)NavDrawMask::TriMesh) {
        const float offset = 0.0115f;
        if (bgfx::isValid(sColorProgram)) {
            std::vector<PosColorVertex> lines; lines.reserve(rt.m_Polys.size() * 6);
            const uint32_t lineColor = packABGR(0.2f, 0.6f, 1.0f, 0.9f);
            auto addLine = [&](uint32_t i0, uint32_t i1){
                glm::vec3 a = rt.m_Vertices[i0] + normals[i0] * offset;
                glm::vec3 b = rt.m_Vertices[i1] + normals[i1] * offset;
                lines.push_back({ a.x, a.y, a.z, lineColor });
                lines.push_back({ b.x, b.y, b.z, lineColor });
            };
            for (const auto& p : rt.m_Polys) {
                addLine(p.i0, p.i1); addLine(p.i1, p.i2); addLine(p.i2, p.i0);
            }
            if (!lines.empty()) {
                const bgfx::Memory* vmem = bgfx::copy(lines.data(), (uint32_t)(lines.size() * sizeof(PosColorVertex)));
                bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(vmem, PosColorVertex::layout);
                float identity[16]; bx::mtxIdentity(identity); bgfx::setTransform(identity);
                bgfx::setVertexBuffer(0, vbh);
                bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_PT_LINES | BGFX_STATE_BLEND_ALPHA);
                bgfx::submit(viewId, sColorProgram);
                bgfx::destroy(vbh);
            }
        } else {
            // Fallback: use existing debug ray lines so at least wireframe is visible
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


