#include "ProjectPanel.h"
#include "rendering/TextureLoader.h"
#include "rendering/ModelLoader.h"
#include <imgui.h>
#include <iostream>
#include <filesystem>
#include <algorithm>
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

    // Handle drag-drop anywhere on the Project panel window\n    if (ImGui::BeginDragDropTarget()) {\n        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {\n            EntityID draggedID = *(EntityID*)payload->Data;\n            // Generate unique prefab path in current folder\n            std::string prefabName = "NewPrefab.prefab";\n            std::string prefabPath = m_CurrentFolder + "/" + prefabName;\n            int counter = 1;\n            while (fs::exists(prefabPath)) {\n                prefabName = "NewPrefab" + std::to_string(counter++) + ".prefab";\n                prefabPath = m_CurrentFolder + "/" + prefabName;\n            }\n            CreatePrefabFromEntity(draggedID, prefabPath);\n        }\n        ImGui::EndDragDropTarget();\n    }\n\n    // --- Navigation Bar ---
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
            std::string prefabName = "NewPrefab.prefab";
            std::string prefabPath = m_CurrentFolder + "/" + prefabName;
            int counter = 1;
            while (fs::exists(prefabPath)) {
                prefabName = "NewPrefab" + std::to_string(counter++) + ".prefab";
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
      // Hide .meta files
      if (!isDir && entry.path().extension() == ".meta") {
         continue;
      }
      EnsureExtraIconsLoaded();
      ImTextureID icon = isDir ? m_FolderIcon : GetFileIconForPath(entry.path().string());

      ImGui::PushID(fileName.c_str());

      // --- Center Icon and align consistently ---
      ImVec2 cursorPos = ImGui::GetCursorPos();
      float iconOffsetX = (cellWidth - thumbnailSize) * 0.5f;
      ImGui::SetCursorPosX(cursorPos.x + iconOffsetX);

      // Always render ImageButton (for both click & drag)
      ImGui::ImageButton(fileName.c_str(), icon, ImVec2(thumbnailSize, thumbnailSize));

      // Click-to-navigate or open
      if (ImGui::IsItemClicked()) {
         m_SelectedItemName = fileName;
         if (isDir) {
            m_CurrentFolder = entry.path().string();
         } else {
            std::string fullPath = entry.path().string();
            if (IsSceneFile(fullPath)) {
               LoadSceneFile(fullPath);
            } else {
               std::cout << "[Open] File clicked: " << fullPath << "\n";
            }
            m_SelectedItemPath = fullPath;
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

      // --- Filename: single-line with ellipsis if overflow ---
      ImGui::SetCursorPosX(cursorPos.x + 2.0f);
      ImGui::PushID((fileName + "_label").c_str());
      ImGui::BeginChild("##label", ImVec2(textWrapWidth, 18.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
      std::string clipped = TruncateWithEllipsis(fileName, textWrapWidth);
      ImGui::TextUnformatted(clipped.c_str());
      ImGui::EndChild();
      ImGui::PopID();

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

