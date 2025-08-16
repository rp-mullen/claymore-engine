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

    // Make toolbar a bit narrower (less tall)
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));

    // Icons rendered at compact size
    ImVec2 iconSize(18, 18);

    // Play button (always enabled)
    if (!m_PlayMode) {
        if (ImGui::ImageButton("##play", m_PlayIcon, iconSize)) {
            TogglePlayMode();
        }
    } else {
        // While in play mode, show Pause and Stop. Pause toggles, Stop exits play mode.
        if (ImGui::ImageButton("##pause", m_PauseIcon, iconSize)) {
            TogglePause();
        }
        ImGui::SameLine();
        if (ImGui::ImageButton("##stop", m_StopIcon, iconSize)) {
            TogglePlayMode();
        }
    }

    ImGui::SameLine();

    // Gizmo toggle
    ImGui::Checkbox("Show Gizmos", &m_ShowGizmos);

    ImGui::SameLine();

    // Gizmo operation mode
    if (ImGui::ImageButton("##move", m_MoveIcon, iconSize)) SetOperation(GizmoOperation::Translate);
    ImGui::SameLine();
    if (ImGui::ImageButton("##rotate", m_RotateIcon, iconSize)) SetOperation(GizmoOperation::Rotate);
    ImGui::SameLine();
    if (ImGui::ImageButton("##scale", m_ScaleIcon, iconSize)) SetOperation(GizmoOperation::Scale);

    ImGui::PopStyleVar();
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
         // Restore global scene pointer immediately to avoid any late calls
         // referencing the destroyed runtime clone in the remainder of the frame
         Scene::CurrentScene = &scene;
         m_UILayer->TogglePlayMode();
         }
      }
   }

void ToolbarPanel::EnsureIconsLoaded() {
    if (m_IconsLoaded) return;
    m_PlayIcon   = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/play.svg"));
    m_PauseIcon  = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/pause.svg"));
    m_StopIcon   = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/stop.svg"));
    m_MoveIcon   = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/move.svg"));
    m_RotateIcon = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/rotate.svg"));
    m_ScaleIcon  = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/scale.svg"));
    m_IconsLoaded = true;
}
