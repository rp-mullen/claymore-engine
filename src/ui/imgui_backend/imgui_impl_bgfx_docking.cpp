#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_impl_bgfx_docking.h"

#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bx/math.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>
#include <algorithm>
#include <cstring>

// ======== BACKEND DATA ========
static uint8_t g_MainViewId = 255;
static bgfx::ProgramHandle g_ShaderHandle = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle g_UniformTex = BGFX_INVALID_HANDLE;
static bgfx::VertexLayout g_VertexLayout;
static bool g_Initialized = false;

// Embedded ImGui shaders
#include "vs_ocornut_imgui.bin.h"
#include "fs_ocornut_imgui.bin.h"
#include "../../bgfx/src/bgfx_p.h"
static const bgfx::EmbeddedShader s_EmbeddedShaders[] = {
    BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER_END()
    };

// Helper: Check transient buffer availability
static bool CheckAvailTransientBuffers(uint32_t numVertices, const bgfx::VertexLayout& layout, uint32_t numIndices) {
    return numVertices == bgfx::getAvailTransientVertexBuffer(numVertices, layout) &&
        (0 == numIndices || numIndices == bgfx::getAvailTransientIndexBuffer(numIndices));
    }

// ======== TEXTURE UPLOAD (NEW ImTextureData MODEL) ========
static void UploadTextureRect(ImTextureData* tex, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    const void* src = tex->GetPixelsAt(x, y);
    const bgfx::Memory* mem = bgfx::makeRef(src, tex->GetPitch() * h);
    bgfx::updateTexture2D({ (uint16_t)tex->GetTexID() }, 0, 0,
        (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h,
        mem, (uint16_t)tex->GetPitch());
    }

static bool ImGui_Implbgfx_UpdateTexture(ImTextureData* tex) {
    if (tex->Status == ImTextureStatus_WantCreate) {
        // Create new BGFX texture
        bgfx::TextureHandle handle = bgfx::createTexture2D(
            (uint16_t)tex->Width, (uint16_t)tex->Height, false, 1,
            bgfx::TextureFormat::RGBA8, 0,
            bgfx::copy(tex->Pixels, tex->BytesPerPixel * tex->Width * tex->Height));

        tex->SetTexID(handle.idx);
        tex->SetStatus(ImTextureStatus_OK);
        return true;
        }
    if (tex->Status == ImTextureStatus_WantUpdates) {
        const bgfx::TextureHandle handle{ (uint16_t)tex->GetTexID() };
        if (!bgfx::isValid(handle)) return false;

        UploadTextureRect(tex, tex->UpdateRect.x, tex->UpdateRect.y, tex->UpdateRect.w, tex->UpdateRect.h);
        for (const auto& a : tex->Updates)
            UploadTextureRect(tex, a.x, a.y, a.w, a.h);

        tex->SetStatus(ImTextureStatus_OK);
        return true;
        }
    if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0) {
        bgfx::TextureHandle handle{ (uint16_t)tex->GetTexID() };
        if (bgfx::isValid(handle)) bgfx::destroy(handle);
        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
        return true;
        }
    return false;
    }

// ======== RENDER FUNCTION ========
void ImGui_ImplBgfx_Render(bgfx::ViewId viewId, ImDrawData* draw_data, uint32_t clearColor) {
    if (!draw_data || draw_data->CmdListsCount == 0)
        return;

    const int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    const int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    // Process ImGui texture updates (font atlas + user textures)
    if (draw_data->Textures) {
        for (ImTextureData* tex : *draw_data->Textures) {
            if (tex->Status != ImTextureStatus_OK)
                ImGui_Implbgfx_UpdateTexture(tex);
            }
        }

    bgfx::touch(viewId);
    if (clearColor)
        bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor, 1.0f, 0);
    bgfx::setViewMode(viewId, bgfx::ViewMode::Sequential);
    bgfx::setViewName(viewId, "ImGui");

    // Setup orthographic projection
    float ortho[16];
    const float L = draw_data->DisplayPos.x;
    const float T = draw_data->DisplayPos.y;
    const float R = L + draw_data->DisplaySize.x;
    const float B = T + draw_data->DisplaySize.y;
    bx::mtxOrtho(ortho, L, R, B, T, 0.0f, 1000.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);

    bgfx::setViewTransform(viewId, nullptr, ortho);
    bgfx::setViewRect(viewId, 0, 0, (uint16_t)fb_width, (uint16_t)fb_height);

    // Render all command lists
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        uint32_t numVertices = (uint32_t)cmd_list->VtxBuffer.size();
        uint32_t numIndices = (uint32_t)cmd_list->IdxBuffer.size();

        if (!CheckAvailTransientBuffers(numVertices, g_VertexLayout, numIndices))
            break;

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;
        bgfx::allocTransientVertexBuffer(&tvb, numVertices, g_VertexLayout);
        bgfx::allocTransientIndexBuffer(&tib, numIndices, sizeof(ImDrawIdx) == 4);

        memcpy(tvb.data, cmd_list->VtxBuffer.Data, numVertices * sizeof(ImDrawVert));
        memcpy(tib.data, cmd_list->IdxBuffer.Data, numIndices * sizeof(ImDrawIdx));

        bgfx::Encoder* encoder = bgfx::begin();
        uint32_t offset = 0;

        for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != cmd_list->CmdBuffer.end(); pcmd++) {
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmd_list, pcmd);
                }
            else if (pcmd->ElemCount > 0) {
                // Compute scissor rectangle
                ImVec4 clip_rect;
                clip_rect.x = (pcmd->ClipRect.x - draw_data->DisplayPos.x) * draw_data->FramebufferScale.x;
                clip_rect.y = (pcmd->ClipRect.y - draw_data->DisplayPos.y) * draw_data->FramebufferScale.y;
                clip_rect.z = (pcmd->ClipRect.z - draw_data->DisplayPos.x) * draw_data->FramebufferScale.x;
                clip_rect.w = (pcmd->ClipRect.w - draw_data->DisplayPos.y) * draw_data->FramebufferScale.y;

                if (clip_rect.x < clip_rect.z && clip_rect.y < clip_rect.w) {
                    encoder->setScissor(
                        (uint16_t)bx::max(clip_rect.x, 0.0f),
                        (uint16_t)bx::max(clip_rect.y, 0.0f),
                        (uint16_t)(clip_rect.z - clip_rect.x),
                        (uint16_t)(clip_rect.w - clip_rect.y));

                    encoder->setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA |
                        BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));

                    bgfx::TextureHandle th = { (uint16_t)(intptr_t)pcmd->GetTexID() };

                    encoder->setTexture(0, g_UniformTex, th);


                    encoder->setVertexBuffer(0, &tvb);
                    encoder->setIndexBuffer(&tib, offset, pcmd->ElemCount);
                    encoder->submit(viewId, g_ShaderHandle);
                    }
                }
            offset += pcmd->ElemCount;
            }
        bgfx::end(encoder);
        }
    }

// ======== DEVICE OBJECTS ========
void ImGui_ImplBgfx_CreateDeviceObjects() {
    const auto type = bgfx::getRendererType();
    g_ShaderHandle = bgfx::createProgram(
        bgfx::createEmbeddedShader(s_EmbeddedShaders, type, "vs_ocornut_imgui"),
        bgfx::createEmbeddedShader(s_EmbeddedShaders, type, "fs_ocornut_imgui"), true);

    g_VertexLayout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    g_UniformTex = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    g_Initialized = true;
    }

void ImGui_ImplBgfx_Init(int view) {
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_bgfx (ImTextureData)";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasViewports;

    g_MainViewId = (uint8_t)view;

    // Multi-viewport support
    ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
    platformIO.Renderer_CreateWindow = nullptr; // Implement if needed
    platformIO.Renderer_DestroyWindow = nullptr;
    platformIO.Renderer_SetWindowSize = nullptr;
    platformIO.Renderer_RenderWindow = nullptr;
    }

void ImGui_ImplBgfx_Shutdown() {
    if (bgfx::isValid(g_ShaderHandle)) bgfx::destroy(g_ShaderHandle);
    if (bgfx::isValid(g_UniformTex)) bgfx::destroy(g_UniformTex);
    g_Initialized = false;
    }

void ImGui_ImplBgfx_NewFrame() {
    if (!g_Initialized)
        ImGui_ImplBgfx_CreateDeviceObjects();
    }
