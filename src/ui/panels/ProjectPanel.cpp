#include "ProjectPanel.h"
#include "rendering/TextureLoader.h"
#include "rendering/ModelLoader.h"
#include <imgui.h>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include "ecs/EntityData.h"

namespace fs = std::filesystem;

ProjectPanel::ProjectPanel(Scene* scene)
   {
   SetContext(scene);
   m_FolderIcon = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIcon("assets/icons/folder.png"));
   m_FileIcon = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIcon("assets/icons/file.png"));
   }

void ProjectPanel::LoadProject(const std::string& projectPath) {
   m_ProjectPath = projectPath;
   m_ProjectRoot = BuildFileTree(projectPath);
   m_CurrentFolder = projectPath;
   }

void ProjectPanel::OnImGuiRender() {
    ImGui::Begin("Project");

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
   
   // Handle drag-drop for creating prefabs
   if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
         EntityID draggedID = *(EntityID*)payload->Data;
         
         // Generate a unique prefab name
         std::string prefabName = "NewPrefab.prefab";
         std::string prefabPath = m_CurrentFolder + "/" + prefabName;
         
         // Ensure unique filename
         int counter = 1;
         while (fs::exists(prefabPath)) {
            prefabName = "NewPrefab" + std::to_string(counter) + ".prefab";
            prefabPath = m_CurrentFolder + "/" + prefabName;
            counter++;
         }
         
         CreatePrefabFromEntity(draggedID, prefabPath);
      }
      ImGui::EndDragDropTarget();
   }
   
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
      ImTextureRef icon = isDir ? m_FolderIcon : m_FileIcon;

      ImGui::PushID(fileName.c_str());

      // --- Center Icon ---
      ImVec2 cursorPos = ImGui::GetCursorPos();
      float iconOffsetX = (cellWidth - thumbnailSize) * 0.5f;
      ImGui::SetCursorPosX(cursorPos.x + iconOffsetX);

      // Always render ImageButton (for both click & drag)
      ImGui::ImageButton(fileName.c_str(), icon, ImVec2(thumbnailSize, thumbnailSize));

      // Click-to-navigate or open
      if (ImGui::IsItemClicked()) {
         if (isDir) {
            m_CurrentFolder = entry.path().string();
         } else {
            std::string fullPath = entry.path().string();
            if (IsSceneFile(fullPath)) {
               LoadSceneFile(fullPath);
            } else {
               std::cout << "[Open] File clicked: " << fullPath << "\n";
            }
         }
      }

      // Double-click for scene files
      if (!isDir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
         std::string fullPath = entry.path().string();
         if (IsSceneFile(fullPath)) {
            LoadSceneFile(fullPath);
         }
      }

      // Drag-drop support
      if (!isDir && ImGui::BeginDragDropSource()) {
         const std::string& fullPath = entry.path().string();
         ImGui::SetDragDropPayload("ASSET_FILE", fullPath.c_str(), fullPath.size() + 1);
         ImGui::Text("Placing: %s", fileName.c_str());
         ImGui::EndDragDropSource();
         }

      // --- Filename (centered & wrapped) ---
      float textWidth = ImGui::CalcTextSize(fileName.c_str()).x;
      float textOffsetX = (cellWidth - textWidth) * 0.5f;
      if (textOffsetX < 0) textOffsetX = 0;

      ImGui::SetCursorPosX(cursorPos.x + textOffsetX);
      ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + textWrapWidth);
      ImGui::TextWrapped("%s", fileName.c_str());
      ImGui::PopTextWrapPos();

      ImGui::NextColumn();
      ImGui::PopID();
      }

   ImGui::Columns(1);
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

