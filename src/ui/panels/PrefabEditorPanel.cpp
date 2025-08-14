#include "PrefabEditorPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <iostream>
#include <bgfx/bgfx.h>
#include <algorithm>
#include "rendering/Renderer.h"
#include "ecs/EntityData.h"
#include "../UILayer.h"

namespace fs = std::filesystem;

PrefabEditorPanel::PrefabEditorPanel(const std::string& prefabPath, UILayer* uiLayer)
    : m_PrefabPath(prefabPath),
      m_UILayer(uiLayer),
      m_ViewportPanel(m_Scene, &m_SelectedEntity, true),
      m_HierarchyPanel(&m_Scene, &m_SelectedEntity),
      m_InspectorPanel(&m_Scene, &m_SelectedEntity)
{
    LoadPrefab(prefabPath);
}

void PrefabEditorPanel::LoadPrefab(const std::string& path)
{
    if (!fs::exists(path)) {
        std::cerr << "[PrefabEditor] Prefab file not found: " << path << std::endl;
        return;
    }

    EntityData prefabData;
    if (!Serializer::LoadPrefabFromFile(path, prefabData, m_Scene)) {
        std::cerr << "[PrefabEditor] Failed to deserialize prefab: " << path << std::endl;
        return;
    }

    // Instantiate entity into the internal scene
    Entity prefabEntity = m_Scene.CreateEntity(prefabData.Name.empty() ? "PrefabEntity" : prefabData.Name);
    EntityData* dstData = m_Scene.GetEntityData(prefabEntity.GetID());
    if (dstData) {
        *dstData = prefabData; // Shallow copy of component pointers; assumes components allocated during deserialization
    }

    m_SelectedEntity = prefabEntity.GetID();
}

void PrefabEditorPanel::OnImGuiRender()
{
    if (!m_IsOpen) return;

    std::string windowTitle = "Prefab Editor - " + fs::path(m_PrefabPath).filename().string();
    if (!ImGui::Begin(windowTitle.c_str(), &m_IsOpen, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }
    // Track whether this window should drive the shared hierarchy/inspector
    m_IsFocusedOrHovered = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
                           ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    // Optional: menu bar for future features
    if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem("Save")) {
            // Reserialize current selected entity back to file
            if (m_SelectedEntity != -1) {
                EntityData* data = m_Scene.GetEntityData(m_SelectedEntity);
                if (data) {
                    Serializer::SavePrefabToFile(*data, m_Scene, m_PrefabPath);
                }
            }
        }
        ImGui::EndMenuBar();
    }

    // Split horizontally: left hierarchy + inspector, right viewport (simple implementation)
    const float leftPaneWidth = 300.0f;
    const float splitterSize = 4.0f;

    // Compute height available inside the prefab editor window. Clamp to >=1 px to satisfy ImGui.
    float fullHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y);

    // Dock this window into main dockspace on first frame
    if (!m_Docked && m_UILayer) {
        ImGuiID dockId = m_UILayer->GetMainDockspaceID();
        if (dockId != 0) {
            ImGui::DockBuilderDockWindow(windowTitle.c_str(), dockId);
            m_Docked = true;
        }
    }

    // Left pane child
    ImGui::BeginChild("HierarchyInspectorPane", ImVec2(leftPaneWidth, fullHeight), true);
    {
        ImGui::TextUnformatted("Hierarchy");
        ImGui::Separator();
        m_HierarchyPanel.OnImGuiRenderEmbedded();
        ImGui::Separator();
        ImGui::TextUnformatted("Inspector");
        ImGui::Separator();
        m_InspectorPanel.OnImGuiRenderEmbedded();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    // Splitter invisible
    ImGui::InvisibleButton("PrefabSplitter", ImVec2(splitterSize, fullHeight));
    if (ImGui::IsItemActive()) {
        // Could implement resizable splitter later
    }

    ImGui::SameLine();

    // Right pane viewport
    ImGui::BeginChild("PrefabViewport", ImVec2(0, fullHeight), true);
    {
        // Render this panel's private scene to its own offscreen texture
        bgfx::TextureHandle tex = Renderer::Get().RenderSceneToTexture(&m_Scene,
            (uint32_t)ImGui::GetContentRegionAvail().x,
            (uint32_t)fullHeight,
            m_ViewportPanel.GetPanelCamera());
        m_ViewportPanel.OnImGuiRenderEmbedded(tex, "PrefabViewportImage");
    }
    ImGui::EndChild();

    ImGui::End();
}
