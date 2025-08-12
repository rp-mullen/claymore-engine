#pragma once

#include <bgfx/bgfx.h>
#include <vector>
#include <glm/mat4x4.hpp>

#include "animation/AnimationEvaluator.h"

struct ImVec2;

namespace cm { namespace animation {

struct PreviewContext {
    uint16_t viewId = 0xff; // assigned at init (e.g., 210)
    bgfx::FrameBufferHandle fb = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle color = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle depth = BGFX_INVALID_HANDLE;
    PoseBuffer pose;
    void Initialize(int width, int height);
    void Resize(int width, int height);
    void Shutdown();
};

} }



