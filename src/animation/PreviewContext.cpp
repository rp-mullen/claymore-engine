#include "animation/PreviewContext.h"
#include <bgfx/bgfx.h>

namespace cm { namespace animation {

void PreviewContext::Initialize(int width, int height)
{
    if (bgfx::isValid(fb)) Shutdown();
    const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    color = bgfx::createTexture2D((uint16_t)width, (uint16_t)height, false, 1, bgfx::TextureFormat::BGRA8, flags);
    depth = bgfx::createTexture2D((uint16_t)width, (uint16_t)height, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT_WRITE_ONLY);
    bgfx::TextureHandle bufs[] = { color, depth };
    fb = bgfx::createFrameBuffer(2, bufs, true);
    // Allocate a dedicated view id if not set; 200+ reserved for editor
    if (viewId == 0xff) viewId = 210;
}

void PreviewContext::Resize(int width, int height)
{
    if (!bgfx::isValid(fb)) { Initialize(width, height); return; }
    Shutdown();
    Initialize(width, height);
}

void PreviewContext::Shutdown()
{
    if (bgfx::isValid(fb)) { bgfx::destroy(fb); fb = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(color)) { bgfx::destroy(color); color = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(depth)) { bgfx::destroy(depth); depth = BGFX_INVALID_HANDLE; }
    pose.local.clear(); pose.touched.clear();
}

} }



