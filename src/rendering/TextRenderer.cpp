#include "TextRenderer.h"
#include "../ecs/Components.h"
#include "../ecs/Scene.h"
#include <glm/gtc/type_ptr.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <imstb_truetype.h>
#include "io/FileSystem.h"

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
    std::vector<uint8_t> ttf;
    if (!FileSystem::Instance().ReadFile(ttfPath, ttf)) {
        // Fallback to stdio if not in pak
        FILE* f = fopen(ttfPath.c_str(), "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        long size = ftell(f); rewind(f);
        ttf.resize(static_cast<size_t>(size));
        fread(ttf.data(), 1, size, f); fclose(f);
    }

    m_Baked.chars.resize(96);
    m_Baked.pixels.assign(w * h, 0);
    int res = stbtt_BakeFontBitmap(reinterpret_cast<const unsigned char*>(ttf.data()), 0, pixelSize, m_Baked.pixels.data(), w, h, 32, 96, (stbtt_bakedchar*)m_Baked.chars.data());
    if (res <= 0) return false;
    m_Baked.width = w; m_Baked.height = h; m_Baked.basePixelSize = pixelSize;
    // Extract font vertical metrics to compute baseline/line height precisely
    stbtt_fontinfo info;
    if (stbtt_InitFont(&info, (const unsigned char*)ttf.data(), stbtt_GetFontOffsetForIndex((const unsigned char*)ttf.data(), 0))) {
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        float scale = stbtt_ScaleForPixelHeight(&info, pixelSize);
        m_Baked.ascentPx = ascent * scale;
        m_Baked.descentPx = -descent * scale; // positive value
        m_Baked.lineGapPx = lineGap * scale;
    }

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

void TextRenderer::SubmitStringScreenWrapped(const TextRendererComponent& tc,
                                             float x, float y,
                                             uint32_t backbufferWidth,
                                             uint32_t backbufferHeight,
                                             bgfx::ViewId viewId) {
    // If no rect or wrap disabled, fallback
    if (tc.RectSize.x <= 0.0f || tc.RectSize.y <= 0.0f || !tc.WordWrap) {
        SubmitStringScreen(tc, x, y, backbufferWidth, backbufferHeight, viewId);
        return;
    }

    const char* text = tc.Text.c_str();
    const float scale = (m_Baked.basePixelSize > 0.0f) ? (tc.PixelSize / m_Baked.basePixelSize) : 1.0f;
    float penx = x, peny = y;
    float maxWidth = tc.RectSize.x;
    float maxHeight = tc.RectSize.y;

    std::vector<Vertex> vertices; vertices.reserve(strlen(text) * 4);
    std::vector<uint16_t> indices; indices.reserve(strlen(text) * 6);
    uint32_t color = tc.ColorAbgr;

    auto emitGlyph = [&](float gx0, float gy0, float gx1, float gy1, float u0, float v0, float u1, float v1) {
        uint16_t base = (uint16_t)vertices.size();
        vertices.push_back({ gx0, gy0, 0.0f, u0, v0, color });
        vertices.push_back({ gx1, gy0, 0.0f, u1, v0, color });
        vertices.push_back({ gx1, gy1, 0.0f, u1, v1, color });
        vertices.push_back({ gx0, gy1, 0.0f, u0, v1, color });
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
    };

    auto measureWord = [&](const char* s)->float {
        float w = 0.0f;
        for (const char* c = s; *c && *c != ' ' && *c != '\n'; ++c) {
            if (*c < 32 || *c >= 128) continue;
            const stbtt_bakedchar* b = ((const stbtt_bakedchar*)m_Baked.chars.data()) + (*c - 32);
            w += b->xadvance * scale;
        }
        return w;
    };

    // Align first baseline inside rect: start pen at top + ascent so glyphs fall within rect
    float ascentScaled = (m_Baked.basePixelSize > 0.0f && m_Baked.ascentPx > 0.0f)
        ? (m_Baked.ascentPx * (tc.PixelSize / m_Baked.basePixelSize))
        : (tc.PixelSize * 0.8f);
    float descentScaled = (m_Baked.basePixelSize > 0.0f && m_Baked.descentPx > 0.0f)
        ? (m_Baked.descentPx * (tc.PixelSize / m_Baked.basePixelSize))
        : (tc.PixelSize * 0.2f);
    float lineGapScaled = (m_Baked.basePixelSize > 0.0f)
        ? (m_Baked.lineGapPx * (tc.PixelSize / m_Baked.basePixelSize))
        : (tc.PixelSize * 0.1f);
    float lineHeight = ascentScaled + descentScaled + lineGapScaled;
    float lineY = y + ascentScaled; // baseline for first line inside rect
    const char* c = text;
    while (*c) {
        if (*c == '\n') { penx = x; lineY += lineHeight; if (lineY > y + maxHeight - descentScaled) break; ++c; continue; }
        if (*c == ' ') {
            float adv = 0.0f; // approximate space advance via baked ' '
            const stbtt_bakedchar* sb = ((const stbtt_bakedchar*)m_Baked.chars.data()) + (' ' - 32);
            adv = sb->xadvance * scale;
            // Wrap if needed
            float nxtWord = measureWord(c + 1);
            if ((penx - x) + adv + nxtWord > maxWidth && nxtWord < maxWidth) {
                penx = x; lineY += lineHeight; if (lineY > y + maxHeight - descentScaled) break; ++c; continue;
            }
            penx += adv; ++c; continue;
        }
        if (*c < 32 || *c >= 128) { ++c; continue; }
        // If the next word doesn't fit on this line, wrap (only at word boundaries)
        float nxtWord = measureWord(c);
        if ((penx - x) + nxtWord > maxWidth && nxtWord < maxWidth) {
            penx = x; lineY += lineHeight; if (lineY > y + maxHeight - descentScaled) break;
            // Skip leading spaces on new line
            while (*c == ' ') ++c;
            if (!*c) break;
        }
        const stbtt_bakedchar* b = ((const stbtt_bakedchar*)m_Baked.chars.data()) + (*c - 32);
        float x0 = penx + b->xoff * scale;
        // yoff is negative above baseline; with baseline set inside rect, glyphs sit fully within
        float y0 = lineY + b->yoff * scale;
        float x1 = x0 + (b->x1 - b->x0) * scale;
        float y1 = y0 + (b->y1 - b->y0) * scale;
        float u0 = b->x0 / float(m_Baked.width);
        float v0 = b->y0 / float(m_Baked.height);
        float u1 = b->x1 / float(m_Baked.width);
        float v1 = b->y1 / float(m_Baked.height);

        // Clip horizontally/vertically to rect
        float rx0 = x, ry0 = y, rx1 = x + maxWidth, ry1 = y + maxHeight;
        if (x1 < rx0 || x0 > rx1 || y1 < ry0 || y0 > ry1) { penx += b->xadvance * scale; ++c; continue; }
        float cgx0 = std::max(x0, rx0);
        float cgy0 = std::max(y0, ry0);
        float cgx1 = std::min(x1, rx1);
        float cgy1 = std::min(y1, ry1);
        float uw = (u1 - u0) / (x1 - x0);
        float vh = (v1 - v0) / (y1 - y0);
        float cu0 = u0 + (cgx0 - x0) * uw;
        float cv0 = v0 + (cgy0 - y0) * vh;
        float cu1 = u1 - (x1 - cgx1) * uw;
        float cv1 = v1 - (y1 - cgy1) * vh;
        emitGlyph(cgx0, cgy0, cgx1, cgy1, cu0, cv0, cu1, cv1);

        penx += b->xadvance * scale;
        ++c;
    }

    if (vertices.empty()) return;

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

void TextRenderer::RenderScreenTexts(const std::vector<std::pair<const TextRendererComponent*, glm::vec2>>& items,
                                     float opacityMultiplier,
                                     uint32_t backbufferWidth,
                                     uint32_t backbufferHeight,
                                     bgfx::ViewId viewId) {
    if (!m_Ready || !bgfx::isValid(m_Program) || !bgfx::isValid(m_Atlas)) return;

    const bgfx::Caps* caps = bgfx::getCaps();
    float ortho[16];
    bx::mtxOrtho(ortho, 0.0f, float(backbufferWidth), float(backbufferHeight), 0.0f, 0.0f, 100.0f, 0.0f, caps->homogeneousDepth);
    float viewIdMat[16]; bx::mtxIdentity(viewIdMat);
    bgfx::setViewTransform(viewId, viewIdMat, ortho);
    bgfx::setViewRect(viewId, 0, 0, (uint16_t)backbufferWidth, (uint16_t)backbufferHeight);

    for (const auto& it : items) {
        const TextRendererComponent* tc = it.first;
        if (!tc) continue;
        if (!tc->Visible || tc->Text.empty()) continue;

        // Temporarily adjust alpha via a local copy of the component's color
        TextRendererComponent temp = *tc;
        uint32_t abgr = temp.ColorAbgr;
        uint8_t a = (uint8_t)((abgr >> 24) & 0xFF);
        float aScaled = (a / 255.0f) * std::max(0.0f, std::min(1.0f, tc->Opacity)) * std::max(0.0f, std::min(1.0f, opacityMultiplier));
        uint8_t aOut = (uint8_t)std::round(std::max(0.0f, std::min(1.0f, aScaled)) * 255.0f);
        temp.ColorAbgr = (uint32_t(aOut) << 24) | (abgr & 0x00FFFFFFu);

        if (temp.WordWrap && temp.RectSize.x > 0.0f && temp.RectSize.y > 0.0f) {
            SubmitStringScreenWrapped(temp, it.second.x, it.second.y, backbufferWidth, backbufferHeight, viewId);
        } else {
            SubmitStringScreen(temp, it.second.x, it.second.y, backbufferWidth, backbufferHeight, viewId);
        }
    }
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

        // Draw world-space texts here; screen-space texts are handled in the UI pass
        if (tc.WorldSpace) {
            SubmitStringWorld(tc, data->Transform.WorldMatrix, worldViewId);
        } else {
            // Defer screen-space rendering to UI pass for correct z-ordering
            continue;
        }
    }
}


