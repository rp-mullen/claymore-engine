#include "animation/PreviewRenderer.h"
#include "animation/PreviewContext.h"
#include <bgfx/bgfx.h>

namespace cm { namespace animation {

void Begin(PreviewContext& ctx, const ImVec2& topLeft, const ImVec2& size)
{
    if (!bgfx::isValid(ctx.fb)) return;
    const uint16_t w = static_cast<uint16_t>(std::max(1.0f, size.x));
    const uint16_t h = static_cast<uint16_t>(std::max(1.0f, size.y));
    bgfx::setViewFrameBuffer(ctx.viewId, ctx.fb);
    bgfx::setViewRect(ctx.viewId, 0, 0, w, h);
    bgfx::setViewClear(ctx.viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x202020ff, 1.0f, 0);
    // Ensure the clear actually executes even if nothing submits
    bgfx::touch(ctx.viewId);
}

void DrawSkeleton(PreviewContext&, const SkeletonComponent&)
{
    // TODO: optional debug draw of bones as lines (requires access to world-space bone transforms)
}

void DrawSkinned(PreviewContext&, const Mesh&, const SkinningData&, const PoseBuffer&)
{
    // TODO: issue skinned mesh draw calls into ctx.viewId using local pose palette
}

void End(PreviewContext& ctx)
{
    // Restore nothing specific; view is isolated by id and fb
    (void)ctx;
}

} }


