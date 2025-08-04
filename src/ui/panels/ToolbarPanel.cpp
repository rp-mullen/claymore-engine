#include "ToolbarPanel.h"
#include "imgui.h"
#include "ui/UILayer.h"

// Renders the main toolbar panel
void ToolbarPanel::OnImGuiRender(ImGuiID dockspace_id) {
    ImGui::Begin("Toolbar");

    // Play/Pause button
    if (ImGui::Button(m_PlayMode ? "Stop" : "Play")) {
        TogglePlayMode();
    }

    ImGui::SameLine();

    // Gizmo toggle
    if (ImGui::Checkbox("Show Gizmos", &m_ShowGizmos)) {
        // Toggle handled by ImGui binding
    }

    ImGui::SameLine();

    // Gizmo operation mode
    if (ImGui::Button("Translate")) SetOperation(GizmoOperation::Translate);
    ImGui::SameLine();
    if (ImGui::Button("Rotate")) SetOperation(GizmoOperation::Rotate);
    ImGui::SameLine();
    if (ImGui::Button("Scale")) SetOperation(GizmoOperation::Scale);

    ImGui::End();
}

void ToolbarPanel::TogglePlayMode() {
   m_PlayMode = !m_PlayMode;

   auto& scene = m_UILayer->GetScene();

   if (m_PlayMode) {
      // Entering Play Mode
      scene.m_RuntimeScene = scene.RuntimeClone();
      scene.m_RuntimeScene->m_IsPlaying = true;
      m_UILayer->TogglePlayMode();
      }
   else {
      // Exiting Play Mode
      if (scene.m_RuntimeScene) {
         scene.m_RuntimeScene->OnStop();
         scene.m_RuntimeScene = nullptr;
         m_UILayer->TogglePlayMode();
         }
      }
   }
