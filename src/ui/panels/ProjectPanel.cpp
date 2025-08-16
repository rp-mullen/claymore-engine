#include "ProjectPanel.h"
#include "rendering/TextureLoader.h"
#include "rendering/ModelLoader.h"
#include <imgui.h>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include "ecs/EntityData.h"
#include "../UILayer.h"

namespace fs = std::filesystem;

namespace {
// Returns a string truncated to fit within maxWidth (in pixels) with an ellipsis suffix when needed
std::string TruncateWithEllipsis(const std::string& text, float maxWidth) {
    if (text.empty()) return text;
    const float fullWidth = ImGui::CalcTextSize(text.c_str()).x;
    if (fullWidth <= maxWidth) return text;

    const float ellipsisWidth = ImGui::CalcTextSize("...").x;
    const float target = std::max(0.0f, maxWidth - ellipsisWidth);

    int low = 0;
    int high = static_cast<int>(text.size());
    int best = 0;
    while (low <= high) {
        int mid = (low + high) / 2;
        float w = ImGui::CalcTextSize(text.substr(0, mid).c_str()).x;
        if (w <= target) { best = mid; low = mid + 1; }
        else { high = mid - 1; }
    }

    std::string out = text.substr(0, best);
    out += "...";
    return out;
}
}

ProjectPanel::ProjectPanel(Scene* scene, UILayer* uiLayer)
   : m_UILayer(uiLayer)
   {
   SetContext(scene);
   m_FolderIcon = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/folder.svg"));
   m_FileIcon = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/file.svg"));
   }

void ProjectPanel::LoadProject(const std::string& projectPath) {
   m_ProjectPath = projectPath;
   m_ProjectRoot = BuildFileTree(projectPath);
   m_CurrentFolder = projectPath;
   }

void ProjectPanel::OnImGuiRender() {
    ImGui::Begin("Project");

    // Handle drag-drop anywhere on the Project panel window
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityID draggedID = *(EntityID*)payload->Data;

            // Prefer filename based on root entity name
            std::string baseName = "Prefab";
            if (m_Context) {
                EntityID rootId = draggedID;
                if (auto* ed = m_Context->GetEntityData(rootId)) {
                    while (ed && ed->Parent != -1) {
                        rootId = ed->Parent;
                        ed = m_Context->GetEntityData(rootId);
                    }
                    if (ed && !ed->Name.empty()) baseName = ed->Name;
                }
            }

            // Sanitize filename
            auto sanitize = [](std::string s) {
                const std::string invalid = "<>:\"/\\|?*";
                for (char& c : s) {
                    if (invalid.find(c) != std::string::npos) c = '_';
                }
                // Trim spaces
                size_t start = s.find_first_not_of(' ');
                size_t end = s.find_last_not_of(' ');
                if (start == std::string::npos) return std::string("Prefab");
                return s.substr(start, end - start + 1);
            };

            std::string desired = sanitize(baseName);
            if (desired.empty()) desired = "Prefab";

            // Ensure unique prefab path in current folder
            std::string prefabName = desired + ".prefab";
            std::string prefabPath = m_CurrentFolder + "/" + prefabName;
            int counter = 1;
            while (fs::exists(prefabPath)) {
                prefabName = desired + "_" + std::to_string(counter++) + ".prefab";
                prefabPath = m_CurrentFolder + "/" + prefabName;
            }
            CreatePrefabFromEntity(draggedID, prefabPath);
        }
        ImGui::EndDragDropTarget();
    }

    // --- Navigation Bar ---
    if (ImGui::Button("< Back") && m_CurrentFolder != m_ProjectPath) {
        m_CurrentFolder = fs::path(m_CurrentFolder).parent_path().string();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(m_CurrentFolder.c_str());
    ImGui::Separator();

    // --- Search Bar ---
    ImGui::PushItemWidth(-1);
    static char searchBuffer[128] = "";
    if (ImGui::InputTextWithHint("##Search", "Search...", searchBuffer, IM_ARRAYSIZE(searchBuffer))) {
        m_SearchQuery = searchBuffer;
    }
    ImGui::PopItemWidth();
    ImGui::Separator();

    // --- Splitter for Folder Tree & File Grid ---
    static float leftWidth = 250.0f;
    float splitterSize = 5.0f;
    float fullWidth = ImGui::GetContentRegionAvail().x;
    float fullHeight = ImGui::GetContentRegionAvail().y;

    // LEFT PANEL
    ImGui::BeginChild("FolderTree", ImVec2(leftWidth, fullHeight), true);
    DrawFolderTree(m_ProjectRoot);
    ImGui::EndChild();

    // SPLITTER
    ImGui::SameLine();
    ImGui::InvisibleButton("Splitter", ImVec2(splitterSize, fullHeight));
    if (ImGui::IsItemActive()) {
        leftWidth += ImGui::GetIO().MouseDelta.x;
        if (leftWidth < 150.0f) leftWidth = 150.0f;
        if (leftWidth > fullWidth - 150.0f) leftWidth = fullWidth - 150.0f;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    ImGui::SameLine();

       // RIGHT PANEL
   ImGui::BeginChild("FileGrid", ImVec2(fullWidth - leftWidth - splitterSize, fullHeight), true);
   
    // Grid-level prefab drop target (background only)
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityID draggedID = *(EntityID*)payload->Data;

            // Prefer filename based on root entity name
            std::string baseName = "Prefab";
            if (m_Context) {
                EntityID rootId = draggedID;
                if (auto* ed = m_Context->GetEntityData(rootId)) {
                    while (ed && ed->Parent != -1) {
                        rootId = ed->Parent;
                        ed = m_Context->GetEntityData(rootId);
                    }
                    if (ed && !ed->Name.empty()) baseName = ed->Name;
                }
            }

            // Sanitize filename
            auto sanitize = [](std::string s) {
                const std::string invalid = "<>:\"/\\|?*";
                for (char& c : s) {
                    if (invalid.find(c) != std::string::npos) c = '_';
                }
                size_t start = s.find_first_not_of(' ');
                size_t end = s.find_last_not_of(' ');
                if (start == std::string::npos) return std::string("Prefab");
                return s.substr(start, end - start + 1);
            };

            std::string desired = sanitize(baseName);
            if (desired.empty()) desired = "Prefab";

            // Ensure unique prefab path in current folder
            std::string prefabName = desired + ".prefab";
            std::string prefabPath = m_CurrentFolder + "/" + prefabName;
            int counter = 1;
            while (fs::exists(prefabPath)) {
                prefabName = desired + "_" + std::to_string(counter++) + ".prefab";
                prefabPath = m_CurrentFolder + "/" + prefabName;
            }
            CreatePrefabFromEntity(draggedID, prefabPath);
        }
        ImGui::EndDragDropTarget();
    }

    // Ensure items render from top-left of grid
    ImVec2 gridStart = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(gridStart);

    
   DrawFileList(m_CurrentFolder);
   ImGui::EndChild();

    ImGui::End();
}





FileNode ProjectPanel::BuildFileTree(const std::string& path) {
   FileNode node;
   node.name = fs::path(path).filename().string();
   node.path = path;
   node.isDirectory = fs::is_directory(path);

   if (node.isDirectory) {
      for (auto& entry : fs::directory_iterator(path)) {
         node.children.push_back(BuildFileTree(entry.path().string()));
         }
      }
   return node;
   }

void ProjectPanel::DrawFolderTree(FileNode& node) {
   ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
      (m_CurrentFolder == node.path ? ImGuiTreeNodeFlags_Selected : 0);

   bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);
   if (ImGui::IsItemClicked()) {
      m_CurrentFolder = node.path;
      }

   if (open) {
      for (auto& child : node.children) {
         if (child.isDirectory) DrawFolderTree(child);
         }
      ImGui::TreePop();
      }
   }

void ProjectPanel::DrawFileList(const std::string& folderPath) {
   if (folderPath.empty() || !fs::exists(folderPath)) return;

   float padding = 10.0f;
   float thumbnailSize = 40.0f;
   float cellWidth = thumbnailSize + padding + 10.0f;
   float textWrapWidth = cellWidth - 4.0f;

   float panelWidth = ImGui::GetContentRegionAvail().x;
   int columnCount = (int)(panelWidth / cellWidth);
   if (columnCount < 1) columnCount = 1;

   ImGui::Columns(columnCount, nullptr, false);
    
   for (auto& entry : fs::directory_iterator(folderPath)) {
      std::string fileName = entry.path().filename().string();
      if (!m_SearchQuery.empty() && fileName.find(m_SearchQuery) == std::string::npos)
         continue;

      bool isDir = entry.is_directory();
      // Hide .meta and model cache binaries
      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      if (!isDir && (ext == ".meta" || ext == ".meshbin" || ext == ".skelbin" || ext == ".animbin")) {
         continue;
      }
      EnsureExtraIconsLoaded();
      ImTextureID icon = isDir ? m_FolderIcon : GetFileIconForPath(entry.path().string());

      ImGui::PushID(fileName.c_str());

      // --- Center Icon and align consistently ---
      ImVec2 cursorPos = ImGui::GetCursorPos();
      float iconOffsetX = (cellWidth - thumbnailSize) * 0.5f;
      ImGui::SetCursorPosX(cursorPos.x + iconOffsetX);

      // Render ImageButton with transparent background (no frame)
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.12f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1,1,1,0.20f));
      ImGui::ImageButton(fileName.c_str(), icon, ImVec2(thumbnailSize, thumbnailSize));
      ImGui::PopStyleColor(3);
      ImGui::PopStyleVar();

      // Single-click: select item; if scene, show inspector info; double-click opens
      if (ImGui::IsItemClicked()) {
         m_SelectedItemName = fileName;
         if (isDir) {
            m_CurrentFolder = entry.path().string();
         } else {
            std::string fullPath = entry.path().string();
            m_SelectedItemPath = fullPath; // selection for inspector
            if (!IsSceneFile(fullPath)) {
               std::cout << "[Open] File clicked: " << fullPath << "\n";
            }
         }
      }

      // Double-click handling for scene and prefab files
      if (!isDir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
         std::string fullPath = entry.path().string();
         if (IsSceneFile(fullPath)) {
            LoadSceneFile(fullPath);
         } else if (IsPrefabFile(fullPath)) {
            if (m_UILayer) {
               m_UILayer->OpenPrefabEditor(fullPath);
            }
         }
      }

      // Drag-drop support
      if (!isDir && ImGui::BeginDragDropSource()) {
         const std::string& fullPath = entry.path().string();
         ImGui::SetDragDropPayload("ASSET_FILE", fullPath.c_str(), fullPath.size() + 1);
         ImGui::Text("Placing: %s", fileName.c_str());
         ImGui::EndDragDropSource();
         }

      // --- Filename: single-line centered under icon, with ellipsis if overflow ---
      std::string clipped = TruncateWithEllipsis(fileName, textWrapWidth);
      float textWidth = ImGui::CalcTextSize(clipped.c_str()).x;
      float textOffsetX = (cellWidth - textWidth) * 0.5f;
      ImGui::SetCursorPosX(cursorPos.x + textOffsetX);
      ImGui::TextUnformatted(clipped.c_str());

      ImGui::NextColumn();
      ImGui::PopID();
      }

   ImGui::Columns(1);
   }

// Pick icons based on file type
ImTextureID ProjectPanel::GetFileIconForPath(const std::string& path) const {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb") return m_Icon3DModel ? m_Icon3DModel : m_FileIcon;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr") return m_IconImage ? m_IconImage : m_FileIcon;
    if (ext == ".scene") return m_FileIcon;
    if (ext == ".prefab") return m_FileIcon;
    return m_FileIcon;
}

// Inspector for the currently selected asset (right panel or inline under grid)
void ProjectPanel::DrawSelectedInspector() {
    if (m_SelectedItemPath.empty()) return;
    const std::string ext = std::filesystem::path(m_SelectedItemPath).extension().string();
    if (IsSceneFile(m_SelectedItemPath)) {
        DrawScenePreviewInspector(m_SelectedItemPath);
    }
}

// Lightweight preview of a scene JSON: counts and referenced asset paths
void ProjectPanel::DrawScenePreviewInspector(const std::string& scenePath) {
    try {
        std::ifstream in(scenePath);
        if (!in.is_open()) return;
        nlohmann::json j; in >> j; in.close();
        int entityCount = 0; if (j.contains("entities") && j["entities"].is_array()) entityCount = (int)j["entities"].size();
        ImGui::Separator();
        ImGui::Text("Scene: %s", std::filesystem::path(scenePath).filename().string().c_str());
        ImGui::Text("Entities: %d", entityCount);
        // Collect asset-looking strings
        std::vector<std::string> assets;
        std::function<void(const nlohmann::json&)> walk = [&](const nlohmann::json& n){
            if (n.is_string()) {
                std::string s = n.get<std::string>();
                std::string lower = s; for(char &c:lower) c = (char)tolower(c);
                if (lower.find("assets/") != std::string::npos || lower.find(".fbx")!=std::string::npos || lower.find(".gltf")!=std::string::npos || lower.find(".png")!=std::string::npos)
                    assets.push_back(s);
            } else if (n.is_array()) for (auto& e : n) walk(e); else if (n.is_object()) for (auto it=n.begin(); it!=n.end(); ++it) walk(it.value());
        };
        walk(j);
        if (!assets.empty()) {
            ImGui::Text("Referenced assets:");
            for (const auto& a : assets) ImGui::BulletText("%s", a.c_str());
        }
    } catch(...) {}
}

void ProjectPanel::EnsureExtraIconsLoaded() const {
    if (m_IconsLoaded) return;
    m_Icon3DModel = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/3d_model.svg"));
    m_IconImage   = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/image.svg"));
    // Optional future: material icon when a material asset type exists
    m_IconsLoaded = true;
}

// Scene and prefab operations
void ProjectPanel::LoadSceneFile(const std::string& filepath) {
   if (!m_Context) {
      std::cerr << "[ProjectPanel] No scene context available for loading\n";
      return;
   }

   if (Serializer::LoadSceneFromFile(filepath, *m_Context)) {
      std::cout << "[ProjectPanel] Successfully loaded scene: " << filepath << std::endl;
   } else {
      std::cerr << "[ProjectPanel] Failed to load scene: " << filepath << std::endl;
   }
}

void ProjectPanel::CreatePrefabFromEntity(EntityID entityId, const std::string& prefabPath) {
   if (!m_Context) {
      std::cerr << "[ProjectPanel] No scene context available for prefab creation\n";
      return;
   }

   auto* entityData = m_Context->GetEntityData(entityId);
   if (!entityData) {
      std::cerr << "[ProjectPanel] Entity not found: " << entityId << std::endl;
      return;
   }

   if (Serializer::SavePrefabToFile(*entityData, *m_Context, prefabPath)) {
      std::cout << "[ProjectPanel] Successfully created prefab: " << prefabPath << std::endl;
      // Refresh the file tree to show the new prefab
      m_ProjectRoot = BuildFileTree(m_ProjectPath);
   } else {
      std::cerr << "[ProjectPanel] Failed to create prefab: " << prefabPath << std::endl;
   }
}

// Helper functions for file operations
bool ProjectPanel::IsSceneFile(const std::string& filepath) const {
   std::string ext = fs::path(filepath).extension().string();
   std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
   return ext == ".scene";
}

bool ProjectPanel::IsPrefabFile(const std::string& filepath) const {
   std::string ext = fs::path(filepath).extension().string();
   std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
   return ext == ".prefab";
}

