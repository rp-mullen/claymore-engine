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

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <rendering/ModelLoader.h>
#include <imgui_internal.h>
#include "ViewportToolbar.h"

// =============================
// Main Viewport Render
// =============================

// =============================================================
// RENDER VIEWPORT PANEL
// =============================================================
void ViewportPanel::OnImGuiRender(bgfx::TextureHandle sceneTexture) {
    ImGui::Begin("Viewport");
    bool viewportActive = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

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


    ImGui::End();
    // Draw transform gizmo
    
    
   }

// =============================================================
// CAMERA CONTROL (Orbit + Zoom)
// =============================================================
void ViewportPanel::HandleCameraControls() {
    Camera* cam = Renderer::Get().GetCamera();
    if (!cam || m_ViewportSize.x <= 0.0f || m_ViewportSize.y <= 0.0f) return;

    ImGuiIO& io = ImGui::GetIO();

    if (ImGuizmo::IsOver()) {
       // Disable mouse/keyboard capture when using gizmo
       io.WantCaptureMouse = true;
       io.WantCaptureKeyboard = true;
       ImGuizmo::Enable(true);
       }

    // Handle Picking
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
       (!ImGuizmo::IsOver() || ImGuizmo::IsUsing())) {
       ImVec2 mousePos = ImGui::GetMousePos();
        // Convert to normalized coordinates inside the letterboxed image
       float nx = (mousePos.x - m_ViewportPos.x) / m_ViewportSize.x;
       float ny = (mousePos.y - m_ViewportPos.y) / m_ViewportSize.y;

       nx = glm::clamp(nx, 0.0f, 1.0f);
       ny = glm::clamp(ny, 0.0f, 1.0f);

       Picking::QueuePick(nx, ny);
       }

    // Camera controls
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
       (!ImGuizmo::IsOver() || ImGuizmo::IsUsing())) {
       io.WantCaptureMouse = false;
       io.WantCaptureKeyboard = false;

       ImVec2 delta = io.MouseDelta;
       m_Yaw += delta.x * 0.2f;
       m_Pitch -= delta.y * 0.2f;
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

    glm::vec3 camPos = m_Target - dir * m_Distance;
    cam->SetPosition(camPos);
    cam->LookAt(m_Target);

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
    Ray ray = Picking::ScreenPointToRay(nx, ny, Renderer::Get().GetCamera());

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

    ImGuizmo::SetOrthographic(false);

    Camera* cam = Renderer::Get().GetCamera();
    if (!cam) return;

    const float* view = glm::value_ptr(cam->GetViewMatrix());
    const float* proj = glm::value_ptr(cam->GetProjectionMatrix());


    glm::mat4 transform =
        glm::translate(glm::mat4(1.0f), data->Transform.Position) *
        glm::yawPitchRoll(glm::radians(data->Transform.Rotation.y),
            glm::radians(data->Transform.Rotation.x),
            glm::radians(data->Transform.Rotation.z)) *
        glm::scale(glm::mat4(1.0f), data->Transform.Scale);

    float matrix[16];
    memcpy(matrix, glm::value_ptr(transform), sizeof(matrix));

    ImGuizmo::Manipulate(view, proj, m_CurrentOperation, m_CurrentMode, matrix);

    if (ImGuizmo::IsUsing()) {
        glm::vec3 pos, rot, scale;
        DecomposeMatrix(matrix, pos, rot, scale);
        data->Transform.Position = pos;
        data->Transform.Rotation = rot;
        data->Transform.Scale = scale;
        // Ensure transform updates propagate to children
        m_Context->MarkTransformDirty(*m_SelectedEntity);
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
   EntityID entityID = m_Context->InstantiateAsset(path, glm::vec3(0.0f));
   if (entityID == -1) {
       std::cerr << "[ViewportPanel] Failed to instantiate asset: " << path << std::endl;
   }

   }

