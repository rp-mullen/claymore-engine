#include "PrefabEditorPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <bgfx/bgfx.h>
#include <algorithm>
#include "rendering/Renderer.h"
#include "ecs/EntityData.h"
#include "../UILayer.h"
#include "pipeline/AssetLibrary.h"
#include "pipeline/AssetMetadata.h"
#include <editor/Project.h>

namespace fs = std::filesystem;

PrefabEditorPanel::PrefabEditorPanel(const std::string& prefabPath, UILayer* uiLayer)
    : m_PrefabPath(prefabPath),
      m_UILayer(uiLayer),
      m_ViewportPanel(m_Scene, &m_SelectedEntity, true)
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
        *dstData = prefabData.DeepCopy(prefabEntity.GetID(), &m_Scene);
    }

    m_SelectedEntity = prefabEntity.GetID();
    // Ensure transforms are up-to-date in the isolated prefab scene
    m_Scene.MarkTransformDirty(m_SelectedEntity);
    m_Scene.UpdateTransforms();
}

void PrefabEditorPanel::OnImGuiRender()
{
    if (!m_IsOpen) return;

    std::string windowTitle = "Prefab Editor - " + fs::path(m_PrefabPath).filename().string();
    if (!ImGui::Begin(windowTitle.c_str(), &m_IsOpen, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }
    // Track whether this window should drive the shared hierarchy/inspector (focus only to avoid flicker)
    m_IsFocusedOrHovered = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    // Optional: menu bar for future features
    if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem("Save")) {
            // Reserialize current selected entity back to file
            if (m_SelectedEntity != -1) {
                EntityData* data = m_Scene.GetEntityData(m_SelectedEntity);
                if (data) {
                    if (Serializer::SavePrefabToFile(*data, m_Scene, m_PrefabPath)) {
                        // Ensure prefab is registered and has a .meta
                        try {
                            std::filesystem::path p(m_PrefabPath);
                            std::string name = p.filename().string();
                            std::error_code ec;
                            std::filesystem::path rel = std::filesystem::relative(p, Project::GetProjectDirectory(), ec);
                            std::string vpath = (ec ? p.string() : rel.string());
                            std::replace(vpath.begin(), vpath.end(), '\\', '/');
                            size_t pos = vpath.find("assets/"); if (pos != std::string::npos) vpath = vpath.substr(pos);
                            std::filesystem::path metaPath = p; metaPath += ".meta";
                            AssetMetadata meta; bool hasMeta = false;
                            if (std::filesystem::exists(metaPath)) {
                                std::ifstream in(metaPath.string()); if (in) { nlohmann::json j; in >> j; in.close(); meta = j.get<AssetMetadata>(); hasMeta = true; }
                            }
                            if (!hasMeta) {
                                meta.guid = ClaymoreGUID::Generate();
                                meta.type = "prefab";
                                nlohmann::json j = meta; std::ofstream out(metaPath.string()); out << j.dump(4); out.close();
                            }
                            AssetLibrary::Instance().RegisterAsset(AssetReference(meta.guid, 0, (int)AssetType::Prefab), AssetType::Prefab, vpath, name);
                            AssetLibrary::Instance().RegisterPathAlias(meta.guid, m_PrefabPath);
                        } catch(...) {}
                    }
                }
            }
        }
        ImGui::EndMenuBar();
    }

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

    // Single child: embedded viewport only; hierarchy/inspector are shared global panels
    ImGui::BeginChild("PrefabViewport", ImVec2(0, fullHeight), true);
    {
        // Update transforms for the isolated prefab scene before rendering
        m_Scene.UpdateTransforms();
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
