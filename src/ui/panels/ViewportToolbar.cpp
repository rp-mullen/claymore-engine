#include "ViewportToolbar.h"
#include "ViewportPanel.h"

#include <imgui.h>
#include <ImGuizmo.h>

void ViewportToolbar::OnImGuiRender() {
    if (!m_Viewport) return;

    // Establish a small floating window. First frame uses default pos; user can drag elsewhere.
    ImGui::SetNextWindowBgAlpha(0.4f);
    ImGui::SetNextWindowSize(ImVec2(40.0f, 120.0f), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_Once);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoDocking;

    ImGui::Begin("##ViewportToolbar", nullptr, flags);

    // Simple manual dragging (since no title bar)
    static bool dragging = false;
    static ImVec2 dragOffset;
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseClicked(0)) {
        dragging = true;
        ImVec2 mouse = ImGui::GetMousePos();
        ImVec2 win  = ImGui::GetWindowPos();
        dragOffset = ImVec2(mouse.x - win.x, mouse.y - win.y);
    }
    if (dragging) {
        if (ImGui::IsMouseDown(0)) {
            ImVec2 mouse = ImGui::GetMousePos();
            ImGui::SetWindowPos(ImVec2(mouse.x - dragOffset.x, mouse.y - dragOffset.y));
        } else {
            dragging = false;
        }
    }

    constexpr float kButtonSize = 24.0f;

    struct GizmoButton {
        ImGuizmo::OPERATION op;
        const char* label;
    } buttons[3] = {
        { ImGuizmo::TRANSLATE, "T" },
        { ImGuizmo::ROTATE,    "R" },
        { ImGuizmo::SCALE,     "S" }
    };

    for (int i = 0; i < 3; ++i) {
        bool active = (m_Viewport->GetCurrentOperation() == buttons[i].op);

        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.55f, 0.92f, 1.0f)); // highlight active button
        }

        if (ImGui::Button(buttons[i].label, ImVec2(kButtonSize, kButtonSize))) {
            m_Viewport->SetOperation(buttons[i].op);
        }

        if (active) {
            ImGui::PopStyleColor();
        }
    }

    ImGui::End();
}
