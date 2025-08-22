#include "ViewportPanel.h"
#include "rendering/Renderer.h"
#include "rendering/Camera.h"
#include "rendering/Picking.h" // New system for ray logic
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstring>
#include "ecs/EntityData.h"
#include <utils/Time.h>
#include <cfloat>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <rendering/ModelLoader.h>
#include <imgui_internal.h>
#include "ViewportToolbar.h"
#include "pipeline/AssetPipeline.h"
#include "core/application.h"
#include <editor/Input.h>
#include "ecs/Scene.h"
#include "ecs/EntityData.h"

// =============================
// Main Viewport Render
// =============================

// =============================================================
// RENDER VIEWPORT PANEL
// =============================================================
void ViewportPanel::OnImGuiRender(bgfx::TextureHandle sceneTexture) {
    ImGui::Begin("Viewport");
    m_WindowFocusedOrHovered = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
                               ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    // Reuse the same rendering path as embedded panels so behavior is identical
    OnImGuiRenderEmbedded(sceneTexture, "MainViewportEmbedded");
    ImGui::End();
}

// Render the viewport inside an existing window/child instead of opening its own window
void ViewportPanel::OnImGuiRenderEmbedded(bgfx::TextureHandle sceneTexture, const char* idLabel) {
    bool viewportActive = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                          ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    // Draw mini viewport toolbar (translate / rotate / scale)
    if (m_Toolbar && viewportActive)
        m_Toolbar->OnImGuiRender();

    // Compute letterboxed viewport to preserve aspect ratio
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float targetAspect = 16.0f / 9.0f; // default aspect if renderer size is unavailable
    int rw = Renderer::Get().GetWidth();
    int rh = Renderer::Get().GetHeight();
    if (rw > 0 && rh > 0) targetAspect = (float)rw / (float)rh;

    float availAspect = (avail.y > 0.0f) ? (avail.x / avail.y) : targetAspect;
    ImVec2 drawSize = avail;
    if (availAspect > targetAspect) {
        // Too wide: pillarbox
        drawSize.x = avail.y * targetAspect;
        drawSize.y = avail.y;
    } else {
        // Too tall: letterbox
        drawSize.x = avail.x;
        drawSize.y = avail.x / targetAspect;
    }

    // Center the image within available region
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 offset = ImVec2((avail.x - drawSize.x) * 0.5f, (avail.y - drawSize.y) * 0.5f);
    ImGui::SetCursorScreenPos(ImVec2(cursor.x + offset.x, cursor.y + offset.y));

    // Draw scene texture
    if (bgfx::isValid(sceneTexture)) {
        ImTextureID texId = (ImTextureID)(uintptr_t)sceneTexture.idx;
        ImGui::PushID(idLabel);
        ImGui::Image(texId, drawSize, ImVec2(0, 0), ImVec2(1, 1));
        // Allow gizmo to receive clicks even though the Image is an item
        ImGui::SetItemAllowOverlap();

        ImGuizmo::BeginFrame();

        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(min.x, min.y, max.x - min.x, max.y - min.y);

        // If the viewport image item is active while hovering the gizmo, release it so the gizmo can capture drag
        if (ImGuizmo::IsOver() && ImGui::IsItemActive() && !ImGuizmo::IsUsing())
        {
            ImGui::ClearActiveID();
        }
        ImGui::PopID();
    }
    else {
        ImGui::Text("Invalid scene texture!");
    }    

    // Store size/pos for input mapping
    m_ViewportSize = drawSize;
    m_ViewportPos = ImGui::GetItemRectMin();

    HandleCameraControls();

    // Handle drag-drop + ghost preview
    HandleAssetDragDrop(ImGui::GetWindowPos());
    if (m_IsDraggingAsset) {
        DrawGhostPreview();
    }

    if (m_IsDraggingAsset && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
       FinalizeAssetDrop();
       m_IsDraggingAsset = false;
       }


    DrawGizmo();
    DrawUIGizmo();
    // Draw all UI rect overlays if enabled
    if (Renderer::Get().GetShowUIRects()) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        DrawUIRectOverlay(dl, m_ViewportPos, m_ViewportSize, m_Context);
    }
}

// =============================================================
// CAMERA CONTROL (Orbit + Zoom)
// =============================================================
void ViewportPanel::HandleCameraControls() {
    Camera* cam = m_UseInternalCamera ? m_Camera.get() : Renderer::Get().GetCamera();
    if (!cam || m_ViewportSize.x <= 0.0f || m_ViewportSize.y <= 0.0f) return;

    // Shortcut: Frame selected (Editor-only)
    if (!Application::Get().IsPlaying() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        // GLFW F key = 70
        if (Input::WasKeyPressedThisFrame(70) && m_SelectedEntity && *m_SelectedEntity != -1) {
            FrameSelected(0.35f);
        }
    }

    ImGuiIO& io = ImGui::GetIO();

    if (ImGuizmo::IsOver()) {
       // Disable mouse/keyboard capture when using gizmo
       io.WantCaptureMouse = true;
       io.WantCaptureKeyboard = true;
       ImGuizmo::Enable(true);
       }

    // Handle Picking
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
       (!ImGuizmo::IsOver() || ImGuizmo::IsUsing()) && !m_UIHandleHovered && !m_UIHandleActive) {
       ImVec2 mousePos = ImGui::GetMousePos();
        // Convert to normalized coordinates inside the letterboxed image
       float nx = (mousePos.x - m_ViewportPos.x) / m_ViewportSize.x;
       float ny = (mousePos.y - m_ViewportPos.y) / m_ViewportSize.y;

       nx = glm::clamp(nx, 0.0f, 1.0f);
       ny = glm::clamp(ny, 0.0f, 1.0f);

       Picking::QueuePick(nx, ny);
       }

    // Report UI mouse position (in framebuffer pixel coords) to renderer every frame
    {
        ImVec2 mouse = ImGui::GetMousePos();
        float mx = (mouse.x - m_ViewportPos.x);
        float my = (mouse.y - m_ViewportPos.y);
        bool inside = (mx >= 0 && my >= 0 && mx <= m_ViewportSize.x && my <= m_ViewportSize.y);
        if (inside) {
            float fbX = mx * (Renderer::Get().GetWidth() / m_ViewportSize.x);
            float fbY = my * (Renderer::Get().GetHeight() / m_ViewportSize.y);
            Renderer::Get().SetUIMousePosition(fbX, fbY, true);
        } else {
            Renderer::Get().SetUIMousePosition(0, 0, false);
        }
    }

    // Camera controls
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
       (!ImGuizmo::IsOver() || ImGuizmo::IsUsing())) {
       io.WantCaptureMouse = false;
       io.WantCaptureKeyboard = false;

       // If mouse is captured (relative mode), use engine input deltas; otherwise use ImGui's
       float dx = 0.0f, dy = 0.0f;
       if (Input::IsRelativeMode()) {
           auto d = Input::GetMouseDelta();
           dx = d.first; dy = d.second;
       } else {
           dx = io.MouseDelta.x; dy = io.MouseDelta.y;
       }
       m_Yaw += dx * 0.2f;
       m_Pitch -= dy * 0.2f;
       m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);

       float scroll = io.MouseWheel;
       m_Distance -= scroll * 0.5f;
       m_Distance = std::max(1.0f, m_Distance);
       }

    if (ImGui::IsWindowHovered() &&
       (!ImGuizmo::IsOver() || ImGuizmo::IsUsing())) {
        float scroll = io.MouseWheel;
        m_Distance -= scroll * 0.5f;
        if (m_Distance < 1.0f) m_Distance = 1.0f;
    }

    // Middle-mouse panning: translate target along camera right/up vectors
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Middle) &&
        (!ImGuizmo::IsOver() || ImGuizmo::IsUsing())) {
        io.WantCaptureMouse = false;
        io.WantCaptureKeyboard = false;

        ImVec2 delta = io.MouseDelta;

        // Compute camera basis from current yaw/pitch
        glm::vec3 forward;
        forward.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        forward.y = sin(glm::radians(m_Pitch));
        forward.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        forward = glm::normalize(forward);

        const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        glm::vec3 right = -1.0f*glm::normalize(glm::cross(forward, worldUp));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        // Scale pan with distance so it feels consistent at different zoom levels
        float panSpeed = std::max(0.001f, m_Distance * 0.01f);

        // Drag right -> move camera right; drag up -> move camera up
        m_Target += (right * delta.x - up * delta.y) * panSpeed;
    }

    glm::vec3 dir;
    dir.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    dir.y = sin(glm::radians(m_Pitch));
    dir.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    dir = glm::normalize(dir);

    // Apply active tween towards target/distance
    if (m_IsTweening) {
        m_TweenTime += (float)Time::GetDeltaTime();
        float t = glm::clamp(m_TweenTime / m_TweenDuration, 0.0f, 1.0f);
        // Smoothstep easing
        t = t * t * (3.0f - 2.0f * t);
        m_Target = glm::mix(m_TargetStart, m_TargetEnd, t);
        m_Distance = glm::mix(m_DistanceStart, m_DistanceEnd, t);
        if (t >= 1.0f) m_IsTweening = false;
    }

    glm::vec3 camPos = m_Target - dir * m_Distance;
    cam->SetPosition(camPos);
    cam->LookAt(m_Target);

}

void ViewportPanel::FrameSelected(float durationSeconds)
{
    if (!m_Context || !m_SelectedEntity || *m_SelectedEntity == -1)
        return;

    auto* data = m_Context->GetEntityData(*m_SelectedEntity);
    if (!data)
        return;

    // Compute world-space AABB by transforming mesh local bounds
    glm::vec3 worldCenter = glm::vec3(data->Transform.WorldMatrix * glm::vec4(0,0,0,1));
    float radius = 1.0f;

    if (data->Mesh && data->Mesh->mesh) {
        glm::vec3 lmin = data->Mesh->mesh->BoundsMin;
        glm::vec3 lmax = data->Mesh->mesh->BoundsMax;
        glm::vec3 corners[8] = {
            {lmin.x,lmin.y,lmin.z},{lmax.x,lmin.y,lmin.z},{lmin.x,lmax.y,lmin.z},{lmax.x,lmax.y,lmin.z},
            {lmin.x,lmin.y,lmax.z},{lmax.x,lmin.y,lmax.z},{lmin.x,lmax.y,lmax.z},{lmax.x,lmax.y,lmax.z}
        };
        glm::vec3 wmin( FLT_MAX), wmax(-FLT_MAX);
        for (int i=0;i<8;i++) {
            glm::vec3 wp = glm::vec3(data->Transform.WorldMatrix * glm::vec4(corners[i], 1.0f));
            wmin = glm::min(wmin, wp);
            wmax = glm::max(wmax, wp);
        }
        worldCenter = (wmin + wmax) * 0.5f;
        glm::vec3 extents = (wmax - wmin) * 0.5f;
        radius = glm::length(extents);
        radius = std::max(radius, 0.001f);
    }

    // Desired distance to frame bounds given current FOV
    float fovDeg = 60.0f;
    if (m_UseInternalCamera && m_Camera) {
        // assume internal cam fov set in ctor
    }
    // Use vertical FOV of 60 deg default; make conservative by using the larger of width/height extents
    float fovRad = glm::radians(60.0f);
    float dist = radius / std::tan(fovRad * 0.5f);
    dist *= 1.2f; // padding

    // Tween from current target/distance
    m_TargetStart = m_Target;
    m_TargetEnd = worldCenter;
    m_DistanceStart = m_Distance;
    m_DistanceEnd = std::max(dist, 0.1f);
    m_TweenDuration = std::max(0.0f, durationSeconds);
    m_TweenTime = 0.0f;
    m_IsTweening = true;
}
// =============================
// Picking
// =============================
void ViewportPanel::HandleEntityPicking() {
    m_ShouldPick = true;

    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 windowPos = ImGui::GetWindowPos();

    float localX = mousePos.x - windowPos.x;
    float localY = mousePos.y - windowPos.y;

    // Normalize coordinates [0..1]
    if (m_ViewportSize.x > 0 && m_ViewportSize.y > 0) {
        m_NormalizedPickX = localX / m_ViewportSize.x;
        m_NormalizedPickY = localY / m_ViewportSize.y;
    }
}


// =============================
// Draw Overlay Grid
// =============================
void ViewportPanel::Draw2DGrid() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 size = m_ViewportSize;

    const float spacing = 32.0f;
    const ImU32 color = IM_COL32(80, 80, 80, 80);

    for (float x = winPos.x; x < winPos.x + size.x; x += spacing)
        drawList->AddLine(ImVec2(x, winPos.y), ImVec2(x, winPos.y + size.y), color);
    for (float y = winPos.y; y < winPos.y + size.y; y += spacing)
        drawList->AddLine(ImVec2(winPos.x, y), ImVec2(winPos.x + size.x, y), color);
}

// =============================
// Drag-Drop Handling
// =============================
void ViewportPanel::HandleAssetDragDrop(const ImVec2& viewportPos) {
    if (!m_Context) return;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
            m_IsDraggingAsset = true;
            m_DraggedAssetPath = (const char*)payload->Data;

            // Compute 3D ghost position
            ImVec2 mousePos = ImGui::GetMousePos();
            UpdateGhostPosition(mousePos.x, mousePos.y);

            // Tooltip
            ImGui::BeginTooltip();
            ImGui::Text("Placing: %s", m_DraggedAssetPath.c_str());
            ImGui::EndTooltip();
        }
        ImGui::EndDragDropTarget();
    }
    else {
        m_IsDraggingAsset = false;
    }
}

// =============================
// Update Ghost Position in World
// =============================
void ViewportPanel::UpdateGhostPosition(float mouseX, float mouseY) {
    // Convert screen coords to normalized within letterboxed viewport
    float nx = (mouseX - m_ViewportPos.x) / m_ViewportSize.x;
    float ny = (mouseY - m_ViewportPos.y) / m_ViewportSize.y;
    Ray ray = Picking::ScreenPointToRay(nx, ny, m_UseInternalCamera ? m_Camera.get() : Renderer::Get().GetCamera());

    if (fabs(ray.Direction.y) > 1e-6f) {
        float t = -ray.Origin.y / ray.Direction.y;
        if (t > 0.0f) {
            glm::vec3 hit = ray.Origin + ray.Direction * t;

            // Snap to grid
            hit.x = round(hit.x / m_GridSize) * m_GridSize;
            hit.z = round(hit.z / m_GridSize) * m_GridSize;
            m_GhostPosition = hit;
        }
    }
}

// =============================
// Draw Ghost Preview (Optional UI)
// =============================
void ViewportPanel::DrawGhostPreview() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 mp = ImGui::GetMousePos();
    dl->AddCircleFilled(mp, 8.0f, IM_COL32(200, 200, 200, 120));
}

// =============================
// ImGuizmo for Transform
// =============================
void ViewportPanel::DrawGizmo() {
    if (*m_SelectedEntity < 0 || !m_Context || !m_ShowGizmos) return;

    // Do not allow gizmo in play mode
    if (m_Context->m_IsPlaying) {
        ImGuizmo::Enable(false);
        return;
    } else {
        ImGuizmo::Enable(true);
    }

    auto* data = m_Context->GetEntityData(*m_SelectedEntity);
    if (!data) return;

    // If selected is a UI element under a screen-space canvas, skip 3D gizmo
    auto* ed = m_Context->GetEntityData(*m_SelectedEntity);
    if (ed) {
        bool isUI = (ed->Panel || ed->Button || (ed->Text && !ed->Text->WorldSpace) || ed->Canvas);
        if (isUI) {
            // climb to check if under screen-space canvas
            EntityID cur = *m_SelectedEntity;
            while (cur != -1) {
                auto* d2 = m_Context->GetEntityData(cur);
                if (!d2) break;
                if (d2->Canvas && d2->Canvas->Space == CanvasComponent::RenderSpace::ScreenSpace) {
                    ImGuizmo::Enable(false);
                    return;
                }
                cur = d2->Parent;
            }
        }
    }

    ImGuizmo::SetOrthographic(false);

        Camera* cam = m_UseInternalCamera ? m_Camera.get() : Renderer::Get().GetCamera();
    if (!cam) return;

        const float* view = glm::value_ptr(cam->GetViewMatrix());
        const float* proj = glm::value_ptr(cam->GetProjectionMatrix());


    // Use world matrix so gizmo appears at the correct spot for children
    // and reflects parent transforms. We'll convert the edited world back
    // into a local matrix relative to the parent below.
    glm::mat4 worldBefore = data->Transform.WorldMatrix;

    float matrix[16];
    memcpy(matrix, glm::value_ptr(worldBefore), sizeof(matrix));

    ImGuizmo::Manipulate(view, proj, m_CurrentOperation, m_CurrentMode, matrix);

    if (ImGuizmo::IsUsing()) {
        // Convert the edited world matrix back into local space
        glm::mat4 editedWorld = glm::make_mat4(matrix);

        glm::mat4 parentWorld = glm::mat4(1.0f);
        if (data->Parent != -1) {
            if (auto* parentData = m_Context->GetEntityData(data->Parent))
                parentWorld = parentData->Transform.WorldMatrix;
        }

        glm::mat4 newLocal = glm::inverse(parentWorld) * editedWorld;

        glm::vec3 pos, rot, scale;
        DecomposeMatrix(glm::value_ptr(newLocal), pos, rot, scale);
        data->Transform.Position = pos;
        data->Transform.Rotation = rot;
        data->Transform.Scale = scale;
        // Ensure transform updates propagate to children
        m_Context->MarkTransformDirty(*m_SelectedEntity);
    }
}

// 2D UI gizmo: draw a draggable handle at panel/text screen position
void ViewportPanel::DrawUIGizmo() {
    if (!m_ShowGizmos || !m_Context || *m_SelectedEntity < 0) return;
    auto* ed = m_Context->GetEntityData(*m_SelectedEntity);
    if (!ed) return;
    bool isScreenUI = false;
    {
        EntityID cur = *m_SelectedEntity;
        while (cur != -1) {
            auto* d2 = m_Context->GetEntityData(cur);
            if (!d2) break;
            if (d2->Canvas && d2->Canvas->Space == CanvasComponent::RenderSpace::ScreenSpace) { isScreenUI = true; break; }
            cur = d2->Parent;
        }
    }
    if (!isScreenUI) return;

    // Determine viewport image top-left to convert screen coords to overlay coords
    ImVec2 viewportTL = m_ViewportPos;
    // Map element position to viewport overlay space: use Panel.Position or Text Transform.Position
    ImVec2 p = viewportTL;
    if (ed->Panel) {
        p.x += ed->Panel->AnchorEnabled ? ed->Panel->AnchorOffset.x : ed->Panel->Position.x;
        p.y += ed->Panel->AnchorEnabled ? ed->Panel->AnchorOffset.y : ed->Panel->Position.y;
    } else if (ed->Text && !ed->Text->WorldSpace) {
        p.x += ed->Transform.Position.x;
        p.y += ed->Transform.Position.y;
    } else if (ed->Canvas) {
        p.x += 0.0f; p.y += 0.0f;
    } else return;

    // Draw handle
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 col = IM_COL32(255, 180, 0, 200);
    float r = 6.0f;
    dl->AddCircleFilled(ImVec2(p.x, p.y), r, col);

    // Drag to move
    ImGui::SetCursorScreenPos(ImVec2(p.x - r, p.y - r));
    ImGui::InvisibleButton("ui_drag", ImVec2(r*2, r*2));
    m_UIHandleHovered = ImGui::IsItemHovered();
    if (m_UIHandleHovered) {
        // Mark UI input as consumed to prevent scene picking
        Renderer::Get().SetUIMousePosition(0,0,true); // keep valid flag
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        m_UIHandleActive = true;
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        // Convert overlay pixel delta to framebuffer pixel delta so it matches renderer's UI space
        float sx = (m_ViewportSize.x > 0.0f) ? (Renderer::Get().GetWidth() / m_ViewportSize.x) : 1.0f;
        float sy = (m_ViewportSize.y > 0.0f) ? (Renderer::Get().GetHeight() / m_ViewportSize.y) : 1.0f;
        delta.x *= sx; delta.y *= sy;
        if (ed->Panel) {
            if (ed->Panel->AnchorEnabled) {
                ed->Panel->AnchorOffset.x += delta.x;
                ed->Panel->AnchorOffset.y += delta.y;
            } else {
                ed->Panel->Position.x += delta.x;
                ed->Panel->Position.y += delta.y;
            }
        } else if (ed->Text && !ed->Text->WorldSpace) {
            ed->Transform.Position.x += delta.x;
            ed->Transform.Position.y += delta.y;
        }
    } else {
        m_UIHandleActive = false;
    }
}

// Helper: draw all UI rects as an editor overlay
void ViewportPanel::DrawUIRectOverlay(ImDrawList* dl, const ImVec2& viewportTL, const ImVec2& viewportSize, Scene* scene) {
    if (!scene) return;
    float sx = (viewportSize.x > 0.0f) ? (Renderer::Get().GetWidth() / viewportSize.x) : 1.0f;
    float sy = (viewportSize.y > 0.0f) ? (Renderer::Get().GetHeight() / viewportSize.y) : 1.0f;
    auto toOverlay = [&](float x, float y){ return ImVec2(viewportTL.x + x / sx, viewportTL.y + y / sy); };
    for (auto& e : scene->GetEntities()) {
        auto* d = scene->GetEntityData(e.GetID()); if (!d || !d->Visible) continue;
        // Check if under a screen-space canvas
        bool underScreen = false;
        {
            EntityID cur = e.GetID();
            while (cur != -1) { auto* d2 = scene->GetEntityData(cur); if (!d2) break; if (d2->Canvas && d2->Canvas->Space == CanvasComponent::RenderSpace::ScreenSpace) { underScreen = true; break; } cur = d2->Parent; }
        }
        if (!underScreen) continue;
        // Panel rect
        if (d->Panel && d->Panel->Visible) {
            float ax = d->Panel->AnchorEnabled ? 0.0f : d->Panel->Position.x;
            float ay = d->Panel->AnchorEnabled ? 0.0f : d->Panel->Position.y;
            if (d->Panel->AnchorEnabled) {
                // Use renderer's backbuffer size for anchoring
                float W = (float)Renderer::Get().GetWidth();
                float H = (float)Renderer::Get().GetHeight();
                switch (d->Panel->Anchor) {
                    case UIAnchorPreset::TopLeft:    break;
                    case UIAnchorPreset::Top:        ax = W * 0.5f; break;
                    case UIAnchorPreset::TopRight:   ax = W; break;
                    case UIAnchorPreset::Left:       ay = H * 0.5f; break;
                    case UIAnchorPreset::Center:     ax = W * 0.5f; ay = H * 0.5f; break;
                    case UIAnchorPreset::Right:      ax = W; ay = H * 0.5f; break;
                    case UIAnchorPreset::BottomLeft: ay = H; break;
                    case UIAnchorPreset::Bottom:     ax = W * 0.5f; ay = H; break;
                    case UIAnchorPreset::BottomRight:ax = W; ay = H; break;
                }
                ax += d->Panel->AnchorOffset.x; ay += d->Panel->AnchorOffset.y;
            }
            float x0 = ax;
            float y0 = ay;
            float x1 = x0 + d->Panel->Size.x * d->Panel->Scale.x;
            float y1 = y0 + d->Panel->Size.y * d->Panel->Scale.y;
            ImU32 col = IM_COL32(255,136,0,160);
            dl->AddRect(toOverlay(x0,y0), toOverlay(x1,y1), col, 0.0f, 0, 1.5f);
        }
        // Text rect: show wrapping rect if set; otherwise approximate baseline rect height using pixel size
        if (d->Text && !d->Text->WorldSpace && d->Text->Visible) {
            float sxp = d->Transform.Position.x; float syp = d->Transform.Position.y;
            if (d->Text->AnchorEnabled) {
                float W = (float)Renderer::Get().GetWidth();
                float H = (float)Renderer::Get().GetHeight();
                switch (d->Text->Anchor) {
                    case UIAnchorPreset::TopLeft:    break;
                    case UIAnchorPreset::Top:        sxp = W * 0.5f; break;
                    case UIAnchorPreset::TopRight:   sxp = W; break;
                    case UIAnchorPreset::Left:       syp = H * 0.5f; break;
                    case UIAnchorPreset::Center:     sxp = W * 0.5f; syp = H * 0.5f; break;
                    case UIAnchorPreset::Right:      sxp = W; syp = H * 0.5f; break;
                    case UIAnchorPreset::BottomLeft: syp = H; break;
                    case UIAnchorPreset::Bottom:     sxp = W * 0.5f; syp = H; break;
                    case UIAnchorPreset::BottomRight:sxp = W; syp = H; break;
                }
                sxp += d->Text->AnchorOffset.x; syp += d->Text->AnchorOffset.y;
            }
            float rw = d->Text->RectSize.x;
            float rh = d->Text->RectSize.y;
            if (rw <= 0.0f || rh <= 0.0f) {
                // Initialize to an approximate AABB for current text (one line height)
                float lineH = d->Text->PixelSize; // approx
                rh = lineH * 1.2f;
                // crude width estimate: characters * 0.6 * pixelSize
                float est = std::max<size_t>(1, d->Text->Text.size());
                rw = std::min(600.0f, est * (d->Text->PixelSize * 0.6f));
            }
            ImU32 col = IM_COL32(0,200,255,160);
            dl->AddRect(toOverlay(sxp, syp), toOverlay(sxp + rw, syp + rh), col, 0.0f, 0, 1.5f);
        }
    }
}

void ViewportPanel::DecomposeMatrix(const float* matrix, glm::vec3& pos, glm::vec3& rot, glm::vec3& scale) {
    glm::mat4 m = glm::make_mat4(matrix);
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat orientation;
    
    // Initialize output parameters
    pos = glm::vec3(0.0f);
    rot = glm::vec3(0.0f);
    scale = glm::vec3(1.0f);
    
    glm::decompose(m, scale, orientation, pos, skew, perspective);
    rot = glm::degrees(glm::eulerAngles(orientation));
}

void ViewportPanel::FinalizeAssetDrop() {
    if (!m_Context || m_DraggedAssetPath.empty()) return;

    std::string path = m_DraggedAssetPath;
    // Models: enqueue background import then hot-swap; others: old path
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb") {
        // Create placeholder
        Entity placeholder = m_Context->CreateEntity("Importing ...");
        EntityID placeholderID = placeholder.GetID();
        if (auto* ed = m_Context->GetEntityData(placeholderID)) {
            ed->Transform.Position = m_GhostPosition;
        }
        AssetPipeline::ImportRequest req;
        req.sourcePath = path;
        req.preferredVPath = "assets/models";
        req.onReady = [this, placeholderID, pos = m_GhostPosition](const BuiltModelPaths& built){
            // If build failed, keep placeholder but rename
            if (built.metaPath.empty()) {
                auto* ed = m_Context->GetEntityData(placeholderID);
                if (ed) ed->Name = "Import failed (see console)";
                return;
            }
            // Replace placeholder
            m_Context->RemoveEntity(placeholderID);
            EntityID id = m_Context->InstantiateModelFast(built.metaPath, pos);
            if (id != -1) *m_SelectedEntity = id;
        };
        if (auto* pipeline = Application::Get().GetAssetPipeline()) {
            pipeline->EnqueueModelImport(req);
        }
    } else {
        // Use the last computed ghost position for placement when available
        EntityID entityID = m_Context->InstantiateAsset(path, m_GhostPosition);
        if (entityID == -1) {
            std::cerr << "[ViewportPanel] Failed to instantiate asset: " << path << std::endl;
        } else {
            *m_SelectedEntity = entityID;
        }
    }
}

