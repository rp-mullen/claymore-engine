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
#include "pipeline/AssetLibrary.h"
#include "pipeline/AssetMetadata.h"
#include <editor/Project.h>
#include "pipeline/MaterialImporter.h"
#include "pipeline/ShaderImporter.h"
#include <glm/glm.hpp>
#include <cstring>

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
    if (!ImGui::Begin("Project")) { ImGui::End(); return; }

    // Handle drag-drop anywhere on the Project panel window
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID", ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
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

            // Ensure we have a valid folder; default to assets/prefabs in project root
            if (m_CurrentFolder.empty()) {
                std::string def = (Project::GetProjectDirectory() / "assets/prefabs").string();
                std::error_code ec; std::filesystem::create_directories(def, ec);
                m_CurrentFolder = def;
            }
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
        if (ImGui::GetDragDropPayload() != nullptr) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }
        ImGui::EndDragDropTarget();
    }

    // --- Navigation Bar ---
    if (ImGui::Button("< Back") && m_CurrentFolder != m_ProjectPath) {
        m_CurrentFolder = fs::path(m_CurrentFolder).parent_path().string();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(m_CurrentFolder.c_str());
    ImGui::SameLine();
    if (ImGui::Button("New Material")) {
        std::string base = "Material";
        std::string outPath = m_CurrentFolder + "/" + base + ".mat";
        int c = 1; while (fs::exists(outPath)) outPath = m_CurrentFolder + "/" + base + "_" + std::to_string(c++) + ".mat";
        CreateMaterialAt(outPath, "");
    }
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
   
    // Grid-level prefab drop target (background)
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID", ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
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

            // Ensure valid folder default
            if (m_CurrentFolder.empty()) {
                std::string def = (Project::GetProjectDirectory() / "assets/prefabs").string();
                std::error_code ec; std::filesystem::create_directories(def, ec);
                m_CurrentFolder = def;
            }
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
        if (ImGui::GetDragDropPayload() != nullptr) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }
        ImGui::EndDragDropTarget();
    }

    // Ensure items render from top-left of grid
    ImVec2 gridStart = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(gridStart);

    
   DrawFileList(m_CurrentFolder);
   ImGui::EndChild();

    ImGui::End();
}





void ProjectPanel::CreateMaterialAt(const std::string& materialPath, const std::string& shaderPath) {
    // Seed a material JSON. If shaderPath provided and points to .shader, set shader and pre-populate defaults via meta when available.
    MaterialAssetUnified mat; mat.name = fs::path(materialPath).stem().string();
    if (!shaderPath.empty()) {
        mat.shaderPath = shaderPath;
        // Prefill params from shader meta defaults
        cm::ShaderMeta meta; std::string err;
        if (cm::ShaderImporter::ExtractMetaFromSource(shaderPath, meta, err)) {
            for (const auto& p : meta.params) {
                glm::vec4 v(0.0f);
                if (!p.defaultValue.empty()) {
                    // parse comma-separated floats
                    std::stringstream ss(p.defaultValue); std::string tok; int idx=0; while (std::getline(ss, tok, ',') && idx<4) {
                        try { v[idx++] = std::stof(tok); } catch(...) {}
                    }
                    if (p.type == "float") { v.y=v.z=v.w=0.0f; }
                }
                mat.params[p.name] = v;
            }
            for (const auto& s : meta.samplers) {
                std::string key = !s.tag.empty() ? s.tag : s.name;
                mat.textures[key] = std::string();
            }
        }
    }
    // Save file
    if (MaterialImporter::Save(materialPath, mat)) {
        std::cout << "[ProjectPanel] Created material: " << materialPath << std::endl;
        // Emit .meta and register in AssetLibrary
        try {
            fs::path p(materialPath);
            fs::path metaPath = p; metaPath += ".meta";
            AssetMetadata meta; bool hasMeta = false;
            if (fs::exists(metaPath)) {
                std::ifstream in(metaPath.string()); if (in) { nlohmann::json j; in >> j; in.close(); meta = j.get<AssetMetadata>(); hasMeta = true; }
            }
            if (!hasMeta) { meta.guid = ClaymoreGUID::Generate(); meta.type = "material"; nlohmann::json j = meta; std::ofstream out(metaPath.string()); out << j.dump(4); }
            std::string name = p.filename().string();
            std::error_code ec; fs::path rel = fs::relative(p, Project::GetProjectDirectory(), ec);
            std::string vpath = (ec ? p.string() : rel.string()); std::replace(vpath.begin(), vpath.end(), '\\', '/');
            size_t pos = vpath.find("assets/"); if (pos != std::string::npos) vpath = vpath.substr(pos);
            AssetLibrary::Instance().RegisterAsset(AssetReference(meta.guid, 0, (int)AssetType::Material), AssetType::Material, vpath, name);
            AssetLibrary::Instance().RegisterPathAlias(meta.guid, materialPath);
        } catch(...) {}
        // Refresh tree
        m_ProjectRoot = BuildFileTree(m_ProjectPath);
    } else {
        std::cerr << "[ProjectPanel] Failed to create material: " << materialPath << std::endl;
    }
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
    
   // Collect entries, group directories first, then files
   struct Entry { fs::directory_entry de; bool isDir; std::string name; };
   std::vector<Entry> entries; entries.reserve(128);
   for (auto& entry : fs::directory_iterator(folderPath)) {
      Entry e{ entry, entry.is_directory(), entry.path().filename().string() };
      entries.push_back(std::move(e));
   }
   std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b){
       if (a.isDir != b.isDir) return a.isDir && !b.isDir; // directories first
       return a.name < b.name; // alphabetical within groups
   });

   for (const auto& item : entries) {
      auto& entry = item.de;
      std::string fileName = entry.path().filename().string();
      if (!m_SearchQuery.empty() && fileName.find(m_SearchQuery) == std::string::npos)
         continue;

      bool isDir = item.isDir;
      // Hide .meta and model/animation cache binaries and avatar files
      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      if (!isDir && (ext == ".meta" || ext == ".meshbin" || ext == ".skelbin" || ext == ".animbin" || ext == ".avatar")) {
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
      if (ImGui::BeginPopupContextItem("file_ctx")) {
          if (ImGui::MenuItem("Create Material")) {
              std::string destFolder = isDir ? entry.path().string() : fs::path(entry.path()).parent_path().string();
              std::string base = "Material";
              std::string outPath = destFolder + "/" + base + ".mat";
              int c=1; while (fs::exists(outPath)) outPath = destFolder + "/" + base + "_" + std::to_string(c++) + ".mat";
              CreateMaterialAt(outPath, "");
          }
          if (ImGui::MenuItem("Rename")) {
              m_PendingRenamePath = entry.path().string();
              m_RenameBuffer = fileName;
              ImGui::OpenPopup("Rename Item");
          }
          if (!isDir && ImGui::MenuItem("Duplicate")) {
              std::string src = entry.path().string();
              std::string dst = src;
              std::string stem = entry.path().stem().string();
              std::string ext  = entry.path().extension().string();
              int c=1; do { dst = (entry.path().parent_path() / (stem + "_copy" + (c>1? ("_"+std::to_string(c)) : std::string()) + ext)).string(); ++c; } while (fs::exists(dst));
              try { fs::copy_file(src, dst); } catch(...) {}
              try {
                  fs::path metaSrc = fs::path(src).string() + ".meta";
                  fs::path metaDst = fs::path(dst).string() + ".meta";
                  if (fs::exists(metaSrc)) {
                      AssetMetadata meta; { std::ifstream in(metaSrc.string()); if (in){ nlohmann::json j; in>>j; meta = j.get<AssetMetadata>(); } }
                      meta.guid = ClaymoreGUID::Generate();
                      nlohmann::json j = meta; std::ofstream out(metaDst.string()); out<<j.dump(4);
                      std::string name = fs::path(dst).filename().string();
                      std::error_code ec; fs::path rel = fs::relative(fs::path(dst), Project::GetProjectDirectory(), ec);
                      std::string vpath = (ec ? dst : rel.string()); std::replace(vpath.begin(), vpath.end(), '\\', '/');
                      size_t pos = vpath.find("assets/"); if (pos != std::string::npos) vpath = vpath.substr(pos);
                      AssetReference aref(meta.guid, 0, (int)ProjectPanel::GuessAssetTypeFromPath(dst));
                      AssetLibrary::Instance().RegisterAsset(aref, ProjectPanel::GuessAssetTypeFromPath(dst), vpath, name);
                      AssetLibrary::Instance().RegisterPathAlias(meta.guid, dst);
                  }
              } catch(...) {}
              m_ProjectRoot = BuildFileTree(m_ProjectPath);
          }
          if (ImGui::MenuItem("Copy")) { m_ClipboardPath = entry.path().string(); m_ClipboardIsCut = false; }
          if (ImGui::MenuItem("Cut"))  { m_ClipboardPath = entry.path().string(); m_ClipboardIsCut = true; }
          if (ImGui::MenuItem("Paste")) {
              std::string destFolder = isDir ? entry.path().string() : fs::path(entry.path()).parent_path().string();
              if (!m_ClipboardPath.empty()) PasteInto(destFolder);
          }
          ImGui::EndPopup();
      }
      // Balance style stack for ImageButton
      ImGui::PopStyleColor(3);
      ImGui::PopStyleVar();
      if (ImGui::BeginPopup("Rename Item")) {
          static char renameBuf[512];
          if (renameBuf[0] == '\0' && !m_RenameBuffer.empty()) {
              std::strncpy(renameBuf, m_RenameBuffer.c_str(), sizeof(renameBuf)-1);
              renameBuf[sizeof(renameBuf)-1] = '\0';
          }
          ImGui::InputText("##rename", renameBuf, sizeof(renameBuf));
          if (ImGui::Button("OK")) {
              if (!m_PendingRenamePath.empty()) {
                  fs::path src = m_PendingRenamePath;
                  m_RenameBuffer = std::string(renameBuf);
                  fs::path dst = src.parent_path() / m_RenameBuffer;
                  try { fs::rename(src, dst); } catch(...) {}
                  // Move sidecar meta and update registry path
                  try {
                      fs::path metaSrc = src.string() + ".meta";
                      fs::path metaDst = dst.string() + ".meta";
                      if (fs::exists(metaSrc)) {
                          // Read GUID
                          AssetMetadata meta; { std::ifstream in(metaSrc.string()); if (in) { nlohmann::json j; in>>j; meta = j.get<AssetMetadata>(); } }
                          fs::rename(metaSrc, metaDst);
                          std::string name = dst.filename().string();
                          std::error_code ec; fs::path rel = fs::relative(dst, Project::GetProjectDirectory(), ec);
                          std::string vpath = (ec ? dst.string() : rel.string()); std::replace(vpath.begin(), vpath.end(), '\\', '/');
                          size_t pos = vpath.find("assets/"); if (pos != std::string::npos) vpath = vpath.substr(pos);
                          AssetType t = ProjectPanel::GuessAssetTypeFromPath(dst.string());
                          AssetReference aref(meta.guid, 0, (int)t);
                          AssetLibrary::Instance().RegisterAsset(aref, t, vpath, name);
                          AssetLibrary::Instance().RegisterPathAlias(meta.guid, dst.string());
                      }
                  } catch(...) {}
                  m_PendingRenamePath.clear();
                  m_RenameBuffer.clear();
                  renameBuf[0] = '\0';
                  m_ProjectRoot = BuildFileTree(m_ProjectPath);
              }
              ImGui::CloseCurrentPopup();
          }
          ImGui::SameLine();
          if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
          ImGui::EndPopup();
      }

      // Single-click: select item; if scene, show inspector info; double-click opens
      if (ImGui::IsItemClicked()) {
         m_SelectedItemName = fileName;
         if (!isDir) {
            std::string fullPath = entry.path().string();
            m_SelectedItemPath = fullPath; // selection for inspector
            if (!IsSceneFile(fullPath)) {
               std::cout << "[Open] File clicked: " << fullPath << "\n";
            }
         }
      }

      // Double-click: enter directory or open asset
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
         if (isDir) {
            m_CurrentFolder = entry.path().string();
         } else {
            std::string fullPath = entry.path().string();
            if (IsSceneFile(fullPath)) {
                if (m_UILayer) m_UILayer->DeferSceneLoad(fullPath);
            } else if (IsPrefabFile(fullPath)) {
                if (m_UILayer) m_UILayer->OpenPrefabEditor(fullPath);
            } else {
                std::string norm = fullPath; std::replace(norm.begin(), norm.end(), '\\', '/');
                std::string extlc = fs::path(norm).extension().string();
                std::transform(extlc.begin(), extlc.end(), extlc.begin(), ::tolower);
                if (extlc == ".json" && norm.find("/assets/prefabs/") != std::string::npos) {
                    if (m_UILayer) m_UILayer->OpenPrefabEditor(fullPath);
                }
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

      // Accept ENTITY_ID or .shader drops on items too (alternate drop target)
      if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID", ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
              EntityID draggedID = *(EntityID*)payload->Data;
              // If dropping onto a file, prefer current folder; default if empty
              if (m_CurrentFolder.empty()) {
                  std::string def = (Project::GetProjectDirectory() / "assets/prefabs").string();
                  std::error_code ec; std::filesystem::create_directories(def, ec);
                  m_CurrentFolder = def;
              }
              // Use file name stem as base
              std::string baseName = fs::path(fileName).stem().string();
              auto sanitize = [](std::string s) {
                  const std::string invalid = "<>:\"/\\|?*";
                  for (char& c : s) { if (invalid.find(c) != std::string::npos) c = '_'; }
                  size_t start = s.find_first_not_of(' ');
                  size_t end = s.find_last_not_of(' ');
                  if (start == std::string::npos) return std::string("Prefab");
                  return s.substr(start, end - start + 1);
              };
              std::string desired = sanitize(baseName);
              if (desired.empty()) desired = "Prefab";
              std::string prefabPath = m_CurrentFolder + "/" + desired + ".prefab";
              int counter = 1;
              while (fs::exists(prefabPath)) {
                  prefabPath = m_CurrentFolder + "/" + desired + "_" + std::to_string(counter++) + ".prefab";
              }
              CreatePrefabFromEntity(draggedID, prefabPath);
          }
          if (const ImGuiPayload* payload2 = ImGui::AcceptDragDropPayload("ASSET_FILE", ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
              const char* dpath = (const char*)payload2->Data;
              if (dpath) {
                  std::string ext2 = fs::path(dpath).extension().string();
                  std::transform(ext2.begin(), ext2.end(), ext2.begin(), ::tolower);
                  if (ext2 == ".shader") {
                      std::string destFolder = isDir ? entry.path().string() : fs::path(entry.path()).parent_path().string();
                      std::string base = fs::path(dpath).stem().string();
                      std::string outPath = destFolder + "/" + base + ".mat";
                      int c=1; while (fs::exists(outPath)) outPath = destFolder + "/" + base + "_" + std::to_string(c++) + ".mat";
                      CreateMaterialAt(outPath, dpath);
                  }
              }
          }
          if (ImGui::GetDragDropPayload() != nullptr) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }
          ImGui::EndDragDropTarget();
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
    std::string norm = path; std::replace(norm.begin(), norm.end(), '\\', '/');
    std::string ext = fs::path(norm).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb") return m_Icon3DModel ? m_Icon3DModel : m_FileIcon;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr") return m_IconImage ? m_IconImage : m_FileIcon;
    if (ext == ".mat") return m_IconMaterial ? m_IconMaterial : m_FileIcon;
    if (ext == ".scene") return m_IconScene ? m_IconScene : m_FileIcon;
    if (ext == ".prefab") return m_IconPrefab ? m_IconPrefab : m_FileIcon;
    if (ext == ".json" && norm.find("/assets/prefabs/") != std::string::npos) return m_IconPrefab ? m_IconPrefab : m_FileIcon;
    if (ext == ".anim") return m_IconAnimation ? m_IconAnimation : m_FileIcon;
    if (ext == ".cs") return m_IconCSharp ? m_IconCSharp : m_FileIcon;
    if (ext == ".animctrl") return m_IconAnimController ? m_IconAnimController : m_FileIcon;
    return m_FileIcon;
}

// Inspector for the currently selected asset (right panel or inline under grid)
void ProjectPanel::DrawSelectedInspector() {
    if (m_SelectedItemPath.empty()) return;
    const std::string ext = std::filesystem::path(m_SelectedItemPath).extension().string();
    if (IsSceneFile(m_SelectedItemPath)) {
        DrawScenePreviewInspector(m_SelectedItemPath);
        return;
    }
    // Material inspector (.mat)
    {
        std::string lower = ext; std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == ".mat") {
            ImGui::Separator();
            ImGui::Text("Material: %s", std::filesystem::path(m_SelectedItemPath).filename().string().c_str());
            MaterialAssetUnified mat{};
            bool ok = MaterialImporter::Load(m_SelectedItemPath, mat);
            if (!ok) {
                ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "Failed to load material JSON");
                return;
            }
            // Shader path field with drag-drop of .shader
            char shaderBuf[512];
            strncpy(shaderBuf, mat.shaderPath.c_str(), sizeof(shaderBuf)); shaderBuf[sizeof(shaderBuf)-1]=0;
            ImGui::InputText("Shader", shaderBuf, sizeof(shaderBuf));
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
                    const char* p = (const char*)payload->Data;
                    if (p) {
                        std::string e = fs::path(p).extension().string(); std::transform(e.begin(), e.end(), e.begin(), ::tolower);
                        if (e == ".shader") {
                            strncpy(shaderBuf, p, sizeof(shaderBuf)); shaderBuf[sizeof(shaderBuf)-1]=0;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
            mat.shaderPath = shaderBuf;
            // If shader set, extract meta to drive UI
            cm::ShaderMeta meta; std::string perr;
            if (!mat.shaderPath.empty()) {
                cm::ShaderImporter::ExtractMetaFromSource(mat.shaderPath, meta, perr);
            }
            // Params UI
            if (!meta.params.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("Parameters");
                for (const auto& p : meta.params) {
                    glm::vec4 v = glm::vec4(0.0f);
                    auto it = mat.params.find(p.name);
                    if (it != mat.params.end()) v = it->second;
                    if (p.uiHint.find("Color") != std::string::npos) {
                        if (ImGui::ColorEdit4(p.name.c_str(), &v.x)) mat.params[p.name] = v;
                    } else {
                        // scalar vs vec4 heuristic
                        if (p.type == "float") {
                            float f = v.x; if (ImGui::DragFloat(p.name.c_str(), &f, 0.01f)) { v.x = f; mat.params[p.name] = v; }
                        } else {
                            if (ImGui::DragFloat4(p.name.c_str(), &v.x, 0.01f)) mat.params[p.name] = v;
                        }
                    }
                }
            }
            // Textures UI
            if (!meta.samplers.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("Textures");
                for (const auto& s : meta.samplers) {
                    std::string key = !s.tag.empty() ? s.tag : s.name;
                    std::string& path = mat.textures[key]; // ensures key exists
                    ImGui::Text("%s", key.c_str()); ImGui::SameLine();
                    char tbuf[512]; strncpy(tbuf, path.c_str(), sizeof(tbuf)); tbuf[sizeof(tbuf)-1]=0;
                    ImGui::InputText((std::string("##tex_") + key).c_str(), tbuf, sizeof(tbuf));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
                            const char* p = (const char*)payload->Data; if (p) { std::string ext2 = fs::path(p).extension().string(); std::transform(ext2.begin(), ext2.end(), ext2.begin(), ::tolower); if (ext2 == ".png" || ext2 == ".jpg" || ext2 == ".jpeg" || ext2 == ".tga" || ext2 == ".bmp" || ext2 == ".hdr") { strncpy(tbuf, p, sizeof(tbuf)); tbuf[sizeof(tbuf)-1]=0; } }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    path = tbuf;
                }
            }
            // Save button
            if (ImGui::Button("Save Material")) {
                MaterialImporter::Save(m_SelectedItemPath, mat);
                std::cout << "[Material] Saved: " << m_SelectedItemPath << std::endl;
            }
            return;
        }
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
    m_IconMaterial= TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/material.svg"));
    m_IconScene   = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/scene.svg"));
    m_IconPrefab  = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/cube.svg"));
    m_IconAnimation = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/animation.svg"));
    m_IconCSharp  = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/csharp.svg"));
    m_IconAnimController = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/anim_controller.svg"));
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
      // Also register prefab in AssetLibrary and emit .meta if missing, so it appears in the Asset Registry
      try {
         fs::path p(prefabPath);
         std::string name = p.filename().string();
         std::error_code ec;
         fs::path rel = fs::relative(p, Project::GetProjectDirectory(), ec);
         std::string vpath = (ec ? p.string() : rel.string());
         std::replace(vpath.begin(), vpath.end(), '\\', '/');
         size_t pos = vpath.find("assets/"); if (pos != std::string::npos) vpath = vpath.substr(pos);
         fs::path metaPath = p; metaPath += ".meta";
         AssetMetadata meta; bool hasMeta = false;
         if (fs::exists(metaPath)) {
            std::ifstream in(metaPath.string()); if (in) { nlohmann::json j; in >> j; in.close(); meta = j.get<AssetMetadata>(); hasMeta = true; }
         }
         if (!hasMeta) {
            meta.guid = ClaymoreGUID::Generate();
            meta.type = "prefab";
            nlohmann::json j = meta; std::ofstream out(metaPath.string()); out << j.dump(4); out.close();
         }
         AssetReference aref(meta.guid, 0, (int)AssetType::Prefab);
         AssetLibrary::Instance().RegisterAsset(aref, AssetType::Prefab, vpath, name);
         AssetLibrary::Instance().RegisterPathAlias(meta.guid, prefabPath);
      } catch(...) {}
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
   std::string norm = filepath; std::replace(norm.begin(), norm.end(), '\\', '/');
   std::string ext = fs::path(norm).extension().string();
   std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
   if (ext == ".prefab") return true;
   if (ext == ".json" && norm.find("/assets/prefabs/") != std::string::npos) return true;
   return false;
}

// Guess asset type from path
AssetType ProjectPanel::GuessAssetTypeFromPath(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr") return AssetType::Texture;
    if (ext == ".mat") return AssetType::Material;
    if (ext == ".anim") return AssetType::Animation;
    if (ext == ".prefab") return AssetType::Prefab;
    if (ext == ".ttf" || ext == ".otf") return AssetType::Font;
    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb") return AssetType::Mesh;
    return AssetType::Shader;
}

void ProjectPanel::PasteInto(const std::string& destFolder) {
    try {
        if (m_ClipboardPath.empty() || destFolder.empty()) return;
        fs::path src = m_ClipboardPath;
        fs::path dst = fs::path(destFolder) / src.filename();
        if (fs::exists(dst)) {
            // disambiguate
            std::string stem = dst.stem().string();
            std::string ext = dst.extension().string();
            int c=1; do { dst = fs::path(destFolder) / (stem + "_" + std::to_string(c++) + ext); } while (fs::exists(dst));
        }
        if (m_ClipboardIsCut) { fs::rename(src, dst); }
        else { fs::copy_file(src, dst); }
        // Move/copy .meta and update registry
        fs::path metaSrc = src.string() + ".meta";
        fs::path metaDst = dst.string() + ".meta";
        if (fs::exists(metaSrc)) {
            AssetMetadata meta; { std::ifstream in(metaSrc.string()); if (in) { nlohmann::json j; in>>j; meta = j.get<AssetMetadata>(); } }
            if (m_ClipboardIsCut) { try { fs::rename(metaSrc, metaDst); } catch(...) {} }
            else { try { fs::copy_file(metaSrc, metaDst); } catch(...) {} }
            std::string name = dst.filename().string();
            std::error_code ec; fs::path rel = fs::relative(dst, Project::GetProjectDirectory(), ec);
            std::string vpath = (ec ? dst.string() : rel.string()); std::replace(vpath.begin(), vpath.end(), '\\', '/');
            size_t pos = vpath.find("assets/"); if (pos != std::string::npos) vpath = vpath.substr(pos);
            AssetType t = ProjectPanel::GuessAssetTypeFromPath(dst.string());
            AssetReference aref(meta.guid, 0, (int)t);
            AssetLibrary::Instance().RegisterAsset(aref, t, vpath, name);
            AssetLibrary::Instance().RegisterPathAlias(meta.guid, dst.string());
        }
        if (m_ClipboardIsCut) { m_ClipboardPath.clear(); m_ClipboardIsCut = false; }
        m_ProjectRoot = BuildFileTree(m_ProjectPath);
    } catch(...) {}
}

