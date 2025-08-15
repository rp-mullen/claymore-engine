#include "TextRenderer.h"
#include "../ecs/Components.h"
#include "../ecs/Scene.h"
#include <glm/gtc/type_ptr.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <imstb_truetype.h>

bgfx::VertexLayout TextRenderer::Vertex::Layout;
void TextRenderer::Vertex::InitLayout() {
    if (Layout.getStride() == 0) {
        Layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();
    }
}

TextRenderer::TextRenderer() {}
TextRenderer::~TextRenderer() {
    if (bgfx::isValid(m_Atlas)) bgfx::destroy(m_Atlas);
    if (bgfx::isValid(m_Sampler)) bgfx::destroy(m_Sampler);
}

bool TextRenderer::BakeFont(const std::string& ttfPath, uint16_t w, uint16_t h, float pixelSize) {
    FILE* f = fopen(ttfPath.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f); rewind(f);
    std::vector<unsigned char> ttf(size);
    fread(ttf.data(), 1, size, f); fclose(f);

    m_Baked.chars.resize(96);
    m_Baked.pixels.assign(w * h, 0);
    int res = stbtt_BakeFontBitmap(ttf.data(), 0, pixelSize, m_Baked.pixels.data(), w, h, 32, 96, (stbtt_bakedchar*)m_Baked.chars.data());
    if (res <= 0) return false;
    m_Baked.width = w; m_Baked.height = h; m_Baked.basePixelSize = pixelSize;

    const bgfx::Memory* mem = bgfx::copy(m_Baked.pixels.data(), w * h);
    if (bgfx::isValid(m_Atlas)) bgfx::destroy(m_Atlas);
    m_Atlas = bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::R8, 0, mem);
    return bgfx::isValid(m_Atlas);
}

bool TextRenderer::Init(const std::string& ttfPath, bgfx::ProgramHandle program, uint16_t atlasWidth, uint16_t atlasHeight, float basePixelSize) {
    Vertex::InitLayout();
    m_Program = program;
    if (!bgfx::isValid(m_Sampler)) m_Sampler = bgfx::createUniform("s_text", bgfx::UniformType::Sampler);
    m_Ready = BakeFont(ttfPath, atlasWidth, atlasHeight, basePixelSize);
    return m_Ready;
}

void TextRenderer::SubmitStringWorld(const TextRendererComponent& tc, const glm::mat4& world, bgfx::ViewId viewId) {
    const char* text = tc.Text.c_str();
    float x = 0.0f, y = 0.0f;
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    vertices.reserve(strlen(text) * 4);
    indices.reserve(strlen(text) * 6);

    uint32_t color = tc.ColorAbgr;
    const float pixelScale = (m_Baked.basePixelSize > 0.0f) ? (tc.PixelSize / m_Baked.basePixelSize) : 1.0f;
    const float unitScale = 0.01f; // Map 100 pixels to 1 world unit to avoid huge glyphs
    const float scale = pixelScale * unitScale;
    for (const char* c = text; *c; ++c) {
        if (*c < 32 || *c >= 128) continue;
        const stbtt_bakedchar* b = ((const stbtt_bakedchar*)m_Baked.chars.data()) + (*c - 32);
        float x0 = x + b->xoff * scale;
        float y0 = y + b->yoff * scale;
        float x1 = x0 + (b->x1 - b->x0) * scale;
        float y1 = y0 + (b->y1 - b->y0) * scale;
        float u0 = b->x0 / float(m_Baked.width);
        float v0 = b->y0 / float(m_Baked.height);
        float u1 = b->x1 / float(m_Baked.width);
        float v1 = b->y1 / float(m_Baked.height);

        uint16_t base = (uint16_t)vertices.size();
        // Flip Y for world space (Y-up world vs baked-down metrics)
        vertices.push_back({ x0, -y0, 0.0f, u0, v0, color });
        vertices.push_back({ x1, -y0, 0.0f, u1, v0, color });
        vertices.push_back({ x1, -y1, 0.0f, u1, v1, color });
        vertices.push_back({ x0, -y1, 0.0f, u0, v1, color });
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);

        x += b->xadvance * scale;
    }

    if (vertices.empty()) return;

    float mtx[16]; memcpy(mtx, glm::value_ptr(world), sizeof(mtx));
    bgfx::setTransform(mtx);
    bgfx::setTexture(0, m_Sampler, m_Atlas);
    const bgfx::Memory* vmem = bgfx::copy(vertices.data(), (uint32_t)(vertices.size() * sizeof(Vertex)));
    const bgfx::Memory* imem = bgfx::copy(indices.data(), (uint32_t)(indices.size() * sizeof(uint16_t)));
    bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(vmem, Vertex::Layout);
    bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(imem);
    bgfx::setVertexBuffer(0, vbh);
    bgfx::setIndexBuffer(ibh);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LEQUAL);
    bgfx::submit(viewId, m_Program);
    bgfx::destroy(vbh);
    bgfx::destroy(ibh);
}

void TextRenderer::SubmitStringScreen(const TextRendererComponent& tc,
                                      float x, float y,
                                      uint32_t backbufferWidth,
                                      uint32_t backbufferHeight,
                                      bgfx::ViewId viewId) {
    const char* text = tc.Text.c_str();
    const float scale = (m_Baked.basePixelSize > 0.0f) ? (tc.PixelSize / m_Baked.basePixelSize) : 1.0f;
    float penx = x, peny = y;
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    vertices.reserve(strlen(text) * 4);
    indices.reserve(strlen(text) * 6);
    uint32_t color = tc.ColorAbgr;

    for (const char* c = text; *c; ++c) {
        if (*c < 32 || *c >= 128) continue;
        const stbtt_bakedchar* b = ((const stbtt_bakedchar*)m_Baked.chars.data()) + (*c - 32);
        float x0 = penx + b->xoff * scale;
        float y0 = peny + b->yoff * scale;
        float x1 = x0 + (b->x1 - b->x0) * scale;
        float y1 = y0 + (b->y1 - b->y0) * scale;
        float u0 = b->x0 / float(m_Baked.width);
        float v0 = b->y0 / float(m_Baked.height);
        float u1 = b->x1 / float(m_Baked.width);
        float v1 = b->y1 / float(m_Baked.height);

        uint16_t base = (uint16_t)vertices.size();
        vertices.push_back({ x0, y0, 0.0f, u0, v0, color });
        vertices.push_back({ x1, y0, 0.0f, u1, v0, color });
        vertices.push_back({ x1, y1, 0.0f, u1, v1, color });
        vertices.push_back({ x0, y1, 0.0f, u0, v1, color });
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);

        penx += b->xadvance * scale;
    }

    if (vertices.empty()) return;

    // Model is identity; view/proj are set by caller for screen view
    float id[16]; bx::mtxIdentity(id); bgfx::setTransform(id);
    bgfx::setTexture(0, m_Sampler, m_Atlas);
    const bgfx::Memory* vmem = bgfx::copy(vertices.data(), (uint32_t)(vertices.size() * sizeof(Vertex)));
    const bgfx::Memory* imem = bgfx::copy(indices.data(), (uint32_t)(indices.size() * sizeof(uint16_t)));
    bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(vmem, Vertex::Layout);
    bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(imem);
    bgfx::setVertexBuffer(0, vbh);
    bgfx::setIndexBuffer(ibh);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA);
    bgfx::submit(viewId, m_Program);
    bgfx::destroy(vbh);
    bgfx::destroy(ibh);
}

void TextRenderer::RenderTexts(Scene& scene,
                               const float* viewMtx,
                               const float* projMtx,
                               uint32_t backbufferWidth,
                               uint32_t backbufferHeight,
                               uint16_t worldViewId,
                               uint16_t screenViewId) {
    if (!m_Ready || !bgfx::isValid(m_Program) || !bgfx::isValid(m_Atlas)) return;

    // For world space texts we assume view/proj already set for worldViewId
    for (auto& e : scene.GetEntities()) {
        auto* data = scene.GetEntityData(e.GetID());
        if (!data || !data->Visible || !data->Text) continue;
        auto& tc = *data->Text;
        if (tc.Text.empty()) continue;

        // If this entity is under a screen-space canvas, render in screen space regardless of Text.WorldSpace
        bool underScreenCanvas = false;
        {
            EntityID cur = e.GetID();
            while (cur != INVALID_ENTITY_ID) {
                auto* d2 = scene.GetEntityData(cur);
                if (!d2) break;
                if (d2->Canvas && d2->Canvas->Space == CanvasComponent::RenderSpace::ScreenSpace) { underScreenCanvas = true; break; }
                cur = d2->Parent;
            }
        }

        if (!underScreenCanvas && tc.WorldSpace) {
            SubmitStringWorld(tc, data->Transform.WorldMatrix, worldViewId);
        } else {
            // Set orthographic view for screen texts
            const bgfx::Caps* caps = bgfx::getCaps();
            float ortho[16];
            bx::mtxOrtho(ortho, 0.0f, float(backbufferWidth), float(backbufferHeight), 0.0f, 0.0f, 100.0f, 0.0f, caps->homogeneousDepth);
            float viewIdMat[16]; bx::mtxIdentity(viewIdMat);
            bgfx::setViewTransform(screenViewId, viewIdMat, ortho);
            bgfx::setViewRect(screenViewId, 0, 0, uint16_t(backbufferWidth), uint16_t(backbufferHeight));
            // Apply text anchoring if enabled
            float sx = data->Transform.Position.x;
            float sy = data->Transform.Position.y;
            if (tc.AnchorEnabled) {
                switch (tc.Anchor) {
                    case UIAnchorPreset::TopLeft: break;
                    case UIAnchorPreset::Top:        sx = backbufferWidth * 0.5f; break;
                    case UIAnchorPreset::TopRight:   sx = (float)backbufferWidth; break;
                    case UIAnchorPreset::Left:       sy = backbufferHeight * 0.5f; break;
                    case UIAnchorPreset::Center:     sx = backbufferWidth * 0.5f; sy = backbufferHeight * 0.5f; break;
                    case UIAnchorPreset::Right:      sx = (float)backbufferWidth; sy = backbufferHeight * 0.5f; break;
                    case UIAnchorPreset::BottomLeft: sy = (float)backbufferHeight; break;
                    case UIAnchorPreset::Bottom:     sx = backbufferWidth * 0.5f; sy = (float)backbufferHeight; break;
                    case UIAnchorPreset::BottomRight:sx = (float)backbufferWidth; sy = (float)backbufferHeight; break;
                }
                sx += tc.AnchorOffset.x;
                sy += tc.AnchorOffset.y;
            }
            SubmitStringScreen(tc, sx, sy, backbufferWidth, backbufferHeight, screenViewId);
        }
    }
}


