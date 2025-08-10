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
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
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
    for (const char* c = text; *c; ++c) {
        if (*c < 32 || *c >= 128) continue;
        const stbtt_bakedchar* b = ((const stbtt_bakedchar*)m_Baked.chars.data()) + (*c - 32);
        float x0 = x + b->xoff;
        float y0 = y + b->yoff;
        float x1 = x0 + (b->x1 - b->x0);
        float y1 = y0 + (b->y1 - b->y0);
        float u0 = b->x0 / float(m_Baked.width);
        float v0 = b->y0 / float(m_Baked.height);
        float u1 = b->x1 / float(m_Baked.width);
        float v1 = b->y1 / float(m_Baked.height);

        uint16_t base = (uint16_t)vertices.size();
        vertices.push_back({ x0, y0, u0, v0, color });
        vertices.push_back({ x1, y0, u1, v0, color });
        vertices.push_back({ x1, y1, u1, v1, color });
        vertices.push_back({ x0, y1, u0, v1, color });
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);

        x += b->xadvance;
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
    float penx = x, peny = y;
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    vertices.reserve(strlen(text) * 4);
    indices.reserve(strlen(text) * 6);
    uint32_t color = tc.ColorAbgr;

    for (const char* c = text; *c; ++c) {
        if (*c < 32 || *c >= 128) continue;
        const stbtt_bakedchar* b = ((const stbtt_bakedchar*)m_Baked.chars.data()) + (*c - 32);
        float x0 = penx + b->xoff;
        float y0 = peny + b->yoff;
        float x1 = x0 + (b->x1 - b->x0);
        float y1 = y0 + (b->y1 - b->y0);
        float u0 = b->x0 / float(m_Baked.width);
        float v0 = b->y0 / float(m_Baked.height);
        float u1 = b->x1 / float(m_Baked.width);
        float v1 = b->y1 / float(m_Baked.height);

        uint16_t base = (uint16_t)vertices.size();
        vertices.push_back({ x0, y0, u0, v0, color });
        vertices.push_back({ x1, y0, u1, v0, color });
        vertices.push_back({ x1, y1, u1, v1, color });
        vertices.push_back({ x0, y1, u0, v1, color });
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);

        penx += b->xadvance;
    }

    if (vertices.empty()) return;

    // Ortho transform for screen space is handled by renderer before calling this
    float id[16];
    id[0]=1; id[1]=0; id[2]=0; id[3]=0; id[4]=0; id[5]=1; id[6]=0; id[7]=0; id[8]=0; id[9]=0; id[10]=1; id[11]=0; id[12]=0; id[13]=0; id[14]=0; id[15]=1;
    bgfx::setTransform(id);
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

        if (tc.WorldSpace) {
            SubmitStringWorld(tc, data->Transform.WorldMatrix, worldViewId);
        } else {
            // Set orthographic view for screen texts
            const bgfx::Caps* caps = bgfx::getCaps();
            float ortho[16];
            bx::mtxOrtho(ortho, 0.0f, float(backbufferWidth), float(backbufferHeight), 0.0f, 0.0f, 100.0f, 0.0f, caps->homogeneousDepth);
            float viewIdMat[16]; bx::mtxIdentity(viewIdMat);
            bgfx::setViewTransform(screenViewId, viewIdMat, ortho);
            bgfx::setViewRect(screenViewId, 0, 0, uint16_t(backbufferWidth), uint16_t(backbufferHeight));
            SubmitStringScreen(tc, data->Transform.Position.x, data->Transform.Position.y, backbufferWidth, backbufferHeight, screenViewId);
        }
    }
}


