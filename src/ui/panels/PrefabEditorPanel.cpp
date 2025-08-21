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
#include "rendering/MaterialManager.h"
#include "rendering/StandardMeshManager.h"

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

    // Load prefab into this panel's private scene using scene-style logic
    // Supports both legacy single-entity and subtree formats without changing serialization
    // Prefer new authoring prefab; fallback to legacy loader
    EntityID root = -1;
    try {
        // Resolve by GUID or path isn’t available here; just call legacy for now if JSON read fails.
        // Future: use PrefabAPI::InstantiatePrefab with resolved GUID
        std::ifstream in(path);
        nlohmann::json j; if (in) { in >> j; in.close(); }
        if (j.is_object() && j.contains("guid") && j.contains("entities")) {
            // Build a tiny scene view from authoring data
            // For now, just create a single root entity stub with name
            std::string name = j.value("name", std::string("Prefab"));
            root = m_Scene.CreateEntityExact(name).GetID();
        }
    } catch(...) {}
    if (root == -1) root = Serializer::LoadPrefabToScene(path, m_Scene);
    if (root == (EntityID)-1 || root == 0) {
        std::cerr << "[PrefabEditor] Failed to load prefab into scene: " << path << std::endl;
        return;
    }

    m_SelectedEntity = root;
    // Ensure transforms are up-to-date in the isolated prefab scene
    m_Scene.MarkTransformDirty(m_SelectedEntity);
    m_Scene.UpdateTransforms();
    // Add non-serialized editor lighting/environment
    EnsureEditorLighting();
}

void PrefabEditorPanel::OnImGuiRender()
{
    if (!m_IsOpen) return;

    // Visible title plus hidden unique ID to avoid collisions when opening same-named prefabs
    std::string displayName = "Prefab Editor - " + fs::path(m_PrefabPath).filename().string();
    std::string windowTitle = displayName + std::string("###PrefabEditor|") + m_PrefabPath;
    if (m_FocusNextFrame) { ImGui::SetNextWindowFocus(); m_FocusNextFrame = false; }
    if (!ImGui::Begin(windowTitle.c_str(), &m_IsOpen, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }
    // Track whether this window should drive the shared hierarchy/inspector (focus only to avoid flicker)
    m_IsFocusedOrHovered = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    // Optional: menu bar for future features
    if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem("Save")) {
            // Save the selected entity and its subtree to preserve hierarchy
            if (m_SelectedEntity != -1) {
                if (Serializer::SavePrefabSubtreeToFile(m_Scene, m_SelectedEntity, m_PrefabPath)) {
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
                } else {
                    std::cerr << "[PrefabEditor] Save failed for: " << m_PrefabPath << std::endl;
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

// Ensure there is editor-only lighting without serializing it into the prefab
void PrefabEditorPanel::EnsureEditorLighting() {
    // Soften ambient to make untextured assets visible
    Environment& env = m_Scene.GetEnvironment();
    env.Ambient = Environment::AmbientMode::FlatColor;
    env.AmbientColor = glm::vec3(0.6f, 0.6f, 0.6f);
    env.AmbientIntensity = 1.0f;
    env.UseSkybox = false;

    // Add a single directional light if none exists
    bool hasAnyLight = false;
    for (const auto& e : m_Scene.GetEntities()) {
        if (auto* d = m_Scene.GetEntityData(e.GetID()); d && d->Light) { hasAnyLight = true; break; }
    }
    if (!hasAnyLight) {
        Entity light = m_Scene.CreateEntityExact("__EditorLight");
        m_EditorLight = light.GetID();
        if (auto* d = m_Scene.GetEntityData(m_EditorLight)) {
            d->Light = std::make_unique<LightComponent>(LightType::Directional, glm::vec3(1.0f), 1.0f);
            d->Transform.Position = glm::vec3(3.0f, 5.0f, 3.0f);
            d->Transform.Rotation = glm::vec3(-45.0f, 45.0f, 0.0f);
        }
    }
}
