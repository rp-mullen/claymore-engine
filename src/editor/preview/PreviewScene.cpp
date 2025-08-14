// PreviewScene.cpp
#include "editor/preview/PreviewScene.h"
#include <bgfx/bgfx.h>
#include "rendering/Renderer.h"
#include "ecs/Scene.h"
#include "rendering/ModelLoader.h"
#include <glm/gtc/matrix_transform.hpp>

bool PreviewScene::Init(int width, int height)
{
    m_Width = width; m_Height = height;
    const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    if (bgfx::isValid(m_FBO)) Shutdown();
    m_Color = bgfx::createTexture2D((uint16_t)width, (uint16_t)height, false, 1, bgfx::TextureFormat::RGBA8, flags);
    // Also create a depth buffer to ensure depth testing works in preview
    bgfx::TextureHandle depth = bgfx::createTexture2D((uint16_t)width, (uint16_t)height, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT_WRITE_ONLY);
    bgfx::TextureHandle bufs[] = { m_Color, depth };
    m_FBO = bgfx::createFrameBuffer(2, bufs, true);
    // Ensure a basic light exists so meshes are visible in the preview
    // Create one directional light if none present
    bool hasLight = false;
    for (const auto& e : m_Scene.GetEntities()) {
        if (auto* d = m_Scene.GetEntityData(e.GetID()); d && d->Light) { hasLight = true; break; }
    }
    if (!hasLight) {
        m_Scene.CreateLight("Preview Light", LightType::Directional, glm::vec3(1.0f), 1.0f);
    }
    return bgfx::isValid(m_FBO);
}

void PreviewScene::Resize(int width, int height)
{
    if (width == m_Width && height == m_Height) return;
    Init(width, height);
}

void PreviewScene::Shutdown()
{
    if (bgfx::isValid(m_FBO)) {
        bgfx::destroy(m_FBO);
        m_FBO = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_Color)) {
        bgfx::destroy(m_Color);
        m_Color = BGFX_INVALID_HANDLE;
    }
}

void PreviewScene::SetModelPath(const std::string& path)
{
    if (m_ModelRoot != -1) m_Scene.RemoveEntity(m_ModelRoot);
    m_ModelRoot = m_Scene.InstantiateModel(path, glm::vec3(0.0f));
}

SkeletonComponent* PreviewScene::GetSkeleton()
{
    if (m_ModelRoot == -1) return nullptr;
    auto* data = m_Scene.GetEntityData(m_ModelRoot);
    if (!data) return nullptr;
    // The skeleton root is created under the model root in Scene import
    for (EntityID child : data->Children) {
        if (auto* cd = m_Scene.GetEntityData(child); cd && cd->Skeleton) return cd->Skeleton.get();
    }
    return nullptr;
}

void PreviewScene::Render(float)
{
    // Render minimal scene if populated
    if (m_ModelRoot != -1) {
        EnsureInView();
        Camera* prevCam = Renderer::Get().GetCamera();
        float yawRad = glm::radians(m_Yaw);
        float pitchRad = glm::radians(m_Pitch);
        glm::vec3 pos = m_CamTarget + glm::vec3(
            m_Distance * cosf(pitchRad) * cosf(yawRad),
            m_Distance * sinf(pitchRad),
            m_Distance * cosf(pitchRad) * sinf(yawRad));
        m_Camera.SetPosition(pos);
        m_Camera.LookAt(m_CamTarget);
        Renderer::Get().SetCamera(&m_Camera);
        // Bind preview framebuffer to view 210 and render the scene using that view, isolated from 0/1
        const uint16_t viewId = m_ViewId;
        bgfx::setViewFrameBuffer(viewId, m_FBO);
        bgfx::setViewRect(viewId, 0, 0, (uint16_t)m_Width, (uint16_t)m_Height);
        bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x202020ff, 1.0f, 0);
        bgfx::touch(viewId);
        // Render with a small fixed viewport size independent of main renderer size
        bgfx::setViewRect(viewId, 0, 0, (uint16_t)m_Width, (uint16_t)m_Height);
        Renderer::Get().RenderScene(m_Scene, viewId);
        // Fallback: if no mesh renders (e.g., materials invalid), draw a default cube/gizmo
        if (m_ModelRoot == -1) {
            // No-op; model should be present
        }
        // Color attachment is the first texture in the FB
        m_Color = GetColorTexture();
        Renderer::Get().SetCamera(prevCam);
    }
}

void PreviewScene::EnsureInView(float padding)
{
    if (m_ModelRoot == -1) return;
    // Update world transforms so bounds can be evaluated in world-space
    m_Scene.UpdateTransforms();

    // Traverse the model hierarchy to compute a conservative world-space AABB
    glm::vec3 bbMin( std::numeric_limits<float>::max());
    glm::vec3 bbMax(-std::numeric_limits<float>::max());

    std::function<void(EntityID)> visit = [&](EntityID id){
        auto* d = m_Scene.GetEntityData(id);
        if (!d) return;
        if (d->Mesh && d->Mesh->mesh) {
            // Transform local mesh AABB corners by world matrix
            const glm::vec3 lmin = d->Mesh->mesh->BoundsMin;
            const glm::vec3 lmax = d->Mesh->mesh->BoundsMax;
            const glm::mat4& M = d->Transform.WorldMatrix;
            const glm::vec3 corners[8] = {
                {lmin.x, lmin.y, lmin.z}, {lmax.x, lmin.y, lmin.z}, {lmin.x, lmax.y, lmin.z}, {lmax.x, lmax.y, lmin.z},
                {lmin.x, lmin.y, lmax.z}, {lmax.x, lmin.y, lmax.z}, {lmin.x, lmax.y, lmax.z}, {lmax.x, lmax.y, lmax.z}
            };
            for (const auto& c : corners) {
                glm::vec3 w = glm::vec3(M * glm::vec4(c, 1.0f));
                bbMin = glm::min(bbMin, w);
                bbMax = glm::max(bbMax, w);
            }
        }
        for (EntityID child : d->Children) visit(child);
    };
    visit(m_ModelRoot);

    if (bbMin.x <= bbMax.x && bbMin.y <= bbMax.y && bbMin.z <= bbMax.z) {
        glm::vec3 center = 0.5f * (bbMin + bbMax);
        glm::vec3 extents = 0.5f * (bbMax - bbMin);
        float radius = glm::max(extents.x, glm::max(extents.y, extents.z));
        m_CamTarget = center;
        // Keep a sane starting distance; clamp to a tight range so we don't zoom out to infinity on huge bounds
        m_Distance = glm::clamp(radius * 2.0f, 1.5f, 10.0f);
    } else {
        // Fallback
        m_CamTarget = glm::vec3(0.0f);
        m_Distance = 3.0f;
    }
}

void PreviewScene::Orbit(float dx, float dy)
{
    m_Yaw += dx * 0.2f;
    m_Pitch = glm::clamp(m_Pitch + dy * 0.2f, -89.0f, 89.0f);
}

void PreviewScene::Dolly(float dz)
{
    m_Distance = glm::clamp(m_Distance - dz * 0.5f, 0.5f, 20.0f);
}

void PreviewScene::Pan(float dx, float dy)
{
    m_CamTarget.x -= dx * 0.01f;
    m_CamTarget.y += dy * 0.01f;
}

void PreviewScene::ResetCamera()
{
    m_CamTarget = glm::vec3(0.0f);
    m_Distance = 3.0f;
    m_Yaw = 0.0f;
    m_Pitch = 15.0f;
}


