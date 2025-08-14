#include "Renderer.h"
#include <bgfx/bgfx.h>

struct OffscreenTarget {
    uint32_t width  = 0;
    uint32_t height = 0;
    bgfx::FrameBufferHandle fb  = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     tex = BGFX_INVALID_HANDLE;
};
static OffscreenTarget g_PrefabTarget;
static const uint16_t kPrefabViewId = 220; // Dedicated offscreen view id for prefab editor

bgfx::TextureHandle Renderer::RenderSceneToTexture(Scene* scene, uint32_t width, uint32_t height, Camera* camera)
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

    // Configure the offscreen view. Do NOT touch views 0/1 used by the main viewport.
    bgfx::setViewFrameBuffer(kPrefabViewId, g_PrefabTarget.fb);
    bgfx::setViewRect(kPrefabViewId, 0, 0, (uint16_t)width, (uint16_t)height);
    bgfx::setViewClear(kPrefabViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x202020ff, 1.0f, 0);
    bgfx::touch(kPrefabViewId);

    // Temporarily swap camera for this render only
    Camera* prevCam = GetCamera();
    if (camera) SetCamera(camera);

    // Render the provided scene entirely to the dedicated view id
    RenderScene(*scene, kPrefabViewId);

    // Restore camera
    if (camera) SetCamera(prevCam);

    return g_PrefabTarget.tex;
}
