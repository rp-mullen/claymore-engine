#include "ToolbarPanel.h"
#include "imgui.h"
#include "ui/UILayer.h"
#include "pipeline/AssetPipeline.h"
#include "ui/Logger.h"
#include "rendering/TextureLoader.h"

// Renders the main toolbar panel
void ToolbarPanel::OnImGuiRender(ImGuiID dockspace_id) {
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);

    EnsureIconsLoaded();

    // Play/Pause button
    ImVec2 iconSize(20, 20);
    if (ImGui::ImageButton("##playstop", m_PlayMode ? m_StopIcon : m_PlayIcon, iconSize)) {
        TogglePlayMode();
    }

    ImGui::SameLine();

    // Gizmo toggle
    if (ImGui::Checkbox("Show Gizmos", &m_ShowGizmos)) {
        // Toggle handled by ImGui binding
    }

    ImGui::SameLine();

    // Gizmo operation mode
    if (ImGui::ImageButton("##move", m_MoveIcon, iconSize)) SetOperation(GizmoOperation::Translate);
    ImGui::SameLine();
    if (ImGui::ImageButton("##rotate", m_RotateIcon, iconSize)) SetOperation(GizmoOperation::Rotate);
    ImGui::SameLine();
    if (ImGui::ImageButton("##scale", m_ScaleIcon, iconSize)) SetOperation(GizmoOperation::Scale);

    ImGui::End();
}

void ToolbarPanel::TogglePlayMode() {
   // Before toggling, ensure scripts compiled
   if(!AssetPipeline::Instance().AreScriptsCompiled()) {
       Logger::LogError("[PlayMode] Cannot enter Play Mode until scripts compile successfully.");
       if(m_UILayer) m_UILayer->FocusConsoleNextFrame();
       return;
   }

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

void ToolbarPanel::EnsureIconsLoaded() {
    if (m_IconsLoaded) return;
    m_PlayIcon   = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/play.svg"));
    m_StopIcon   = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/pause.svg"));
    m_MoveIcon   = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/move.svg"));
    m_RotateIcon = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/rotate.svg"));
    m_ScaleIcon  = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/scale.svg"));
    m_IconsLoaded = true;
}
