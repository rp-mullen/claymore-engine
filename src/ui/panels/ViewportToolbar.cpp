#include "ViewportToolbar.h"
#include "ViewportPanel.h"

#include <imgui.h>
#include <ImGuizmo.h>

void ViewportToolbar::OnImGuiRender() {
    if (!m_Viewport) return;

    // Always anchor to the top-left of the Viewport panel's content region
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::SetNextWindowSize(ImVec2(40.0f, 120.0f), ImGuiCond_Once);
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 anchor = ImVec2(winPos.x + contentMin.x + 8.0f, winPos.y + contentMin.y + 28.0f); // +20px vertical offset
    ImGui::SetNextWindowPos(anchor, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoMove;

    ImGui::Begin("##ViewportToolbar", nullptr, flags);

    constexpr float kButtonSize = 26.0f;

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
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.55f, 0.92f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.58f, 0.96f, 1.0f));
        }

        if (ImGui::Button(buttons[i].label, ImVec2(kButtonSize, kButtonSize))) {
            m_Viewport->SetOperation(buttons[i].op);
        }

        if (active) {
            ImGui::PopStyleColor(2);
        }
    }

    ImGui::End();
}
