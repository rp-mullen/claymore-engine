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

        const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_UVW_CLAMP;
        g_PrefabTarget.tex = bgfx::createTexture2D((uint16_t)width, (uint16_t)height, false, 1, bgfx::TextureFormat::RGBA8, flags);
        g_PrefabTarget.fb  = bgfx::createFrameBuffer(1, &g_PrefabTarget.tex, true);
        g_PrefabTarget.width  = width;
        g_PrefabTarget.height = height;
    }

    const uint16_t viewId = 5; // pick an unused view id
    bgfx::setViewFrameBuffer(viewId, g_PrefabTarget.fb);
    bgfx::setViewRect(viewId, 0, 0, width, height);
    bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x404040ff, 1.0f, 0);

    // Temporarily switch global scene pointer
    Scene* prev = Scene::CurrentScene;
    Scene::CurrentScene = scene;
    RenderScene(*scene);
    Scene::CurrentScene = prev;

    return g_PrefabTarget.tex;
}
