#pragma once
#include <bgfx/bgfx.h>
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <imstb_truetype.h>

// Lightweight text rendering using stb_truetype baked atlas
// Bakes ASCII 32..126 from a TTF into an R8 atlas and renders with a simple shader

class Scene;
struct TextRendererComponent;

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    // Initialize with default font path and shader program
    bool Init(const std::string& ttfPath, bgfx::ProgramHandle program, uint16_t atlasWidth = 512, uint16_t atlasHeight = 512, float basePixelSize = 48.0f);

    // Render all TextRendererComponent instances in the scene
    void RenderTexts(Scene& scene,
                     const float* viewMtx,
                     const float* projMtx,
                     uint32_t backbufferWidth,
                     uint32_t backbufferHeight,
                     uint16_t worldViewId = 1,
                     uint16_t screenViewId = 0);

    // Render only screen-space texts with given order (already sorted) and apply opacity multiplier
    void RenderScreenTexts(const std::vector<std::pair<const TextRendererComponent*, glm::vec2>>& items,
                           float opacityMultiplier,
                           uint32_t backbufferWidth,
                           uint32_t backbufferHeight,
                           bgfx::ViewId screenViewId);

    // Submit one screen-space text with extra opacity multiplier
    void SubmitStringScreenWithOpacity(const TextRendererComponent& tc,
                                       float x, float y,
                                       uint32_t backbufferWidth,
                                       uint32_t backbufferHeight,
                                       float opacityMultiplier,
                                       bgfx::ViewId viewId);

private:
    struct Baked {
        std::vector<unsigned char> pixels; // R8 atlas
        std::vector<stbtt_bakedchar> chars; // 95 glyphs (32..126)
        uint16_t width = 0;
        uint16_t height = 0;
        float basePixelSize = 48.0f;
        // Font vertical metrics at basePixelSize (pixels)
        float ascentPx = 0.0f;
        float descentPx = 0.0f;
        float lineGapPx = 0.0f;
    };

    struct Vertex {
        float x, y, z;
        float u, v;
        uint32_t abgr;
        static bgfx::VertexLayout Layout;
        static void InitLayout();
    };

    bool BakeFont(const std::string& ttfPath, uint16_t w, uint16_t h, float pixelSize);

    void SubmitStringWorld(const TextRendererComponent& tc,
                           const glm::mat4& world,
                           bgfx::ViewId viewId);

    void SubmitStringScreen(const TextRendererComponent& tc,
                            float x, float y,
                            uint32_t backbufferWidth,
                            uint32_t backbufferHeight,
                            bgfx::ViewId viewId);

    void SubmitStringScreenWrapped(const TextRendererComponent& tc,
                                   float x, float y,
                                   uint32_t backbufferWidth,
                                   uint32_t backbufferHeight,
                                   bgfx::ViewId viewId);

    bgfx::TextureHandle m_Atlas = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_Sampler = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_Program = BGFX_INVALID_HANDLE;

    Baked m_Baked;
    bool m_Ready = false;
};


