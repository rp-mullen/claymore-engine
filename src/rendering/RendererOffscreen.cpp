#include "Renderer.h"
#include <bgfx/bgfx.h>

struct OffscreenTarget {
    uint32_t width  = 0;
    uint32_t height = 0;
    bgfx::FrameBufferHandle fb  = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     tex = BGFX_INVALID_HANDLE;
};
static OffscreenTarget g_PrefabTarget;

bgfx::TextureHandle Renderer::RenderSceneToTexture(Scene* scene, uint32_t width, uint32_t height)
{
    if (!scene || width == 0 || height == 0)
        return BGFX_INVALID_HANDLE;

    // (Re)create target if size changed
    if (!bgfx::isValid(g_PrefabTarget.fb) || g_PrefabTarget.width != width || g_PrefabTarget.height != height)
    {
        if (bgfx::isValid(g_PrefabTarget.fb))
            bgfx::destroy(g_PrefabTarget.fb);
        if (bgfx::isValid(g_PrefabTarget.tex))
            bgfx::destroy(g_PrefabTarget.tex);

        const uint64_t colorFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
        const uint64_t depthFlags = BGFX_TEXTURE_RT_WRITE_ONLY;
        g_PrefabTarget.tex = bgfx::createTexture2D((uint16_t)width, (uint16_t)height, false, 1, bgfx::TextureFormat::RGBA8, colorFlags);
        bgfx::TextureHandle depth = bgfx::createTexture2D((uint16_t)width, (uint16_t)height, false, 1, bgfx::TextureFormat::D24S8, depthFlags);
        bgfx::TextureHandle attachments[] = { g_PrefabTarget.tex, depth };
        g_PrefabTarget.fb  = bgfx::createFrameBuffer(2, attachments, true);
        g_PrefabTarget.width  = width;
        g_PrefabTarget.height = height;
    }

    // Route our standard views to this offscreen target for the duration of this render
    // Save main renderer dimensions
    uint32_t prevW = m_Width;
    uint32_t prevH = m_Height;
    bgfx::FrameBufferHandle prevFB = m_SceneFrameBuffer;
    float prevViewM[16]; memcpy(prevViewM, m_view, sizeof(m_view));
    float prevProjM[16]; memcpy(prevProjM, m_proj, sizeof(m_proj));

    // Quick unblock: redirect view 1 (where RenderScene submits) onto the preview framebuffer
    bgfx::setViewFrameBuffer(1, g_PrefabTarget.fb);
    bgfx::setViewRect(1, 0, 0, (uint16_t)width, (uint16_t)height);
    bgfx::setViewClear(1, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x202020ff, 1.0f, 0);
    bgfx::touch(1);

    // Temporarily switch global scene pointer
    Scene* prev = Scene::CurrentScene;
    Scene::CurrentScene = scene;
    RenderScene(*scene);
    Scene::CurrentScene = prev;

    // Restore main framebuffer and leave preview view bound only to offscreen
    bgfx::setViewFrameBuffer(0, prevFB);
    bgfx::setViewFrameBuffer(1, prevFB);
    bgfx::setViewRect(0, 0, 0, prevW, prevH);
    bgfx::setViewRect(1, 0, 0, prevW, prevH);
    bgfx::setViewTransform(0, prevViewM, prevProjM);
    bgfx::setViewTransform(1, prevViewM, prevProjM);

    return g_PrefabTarget.tex;
}
