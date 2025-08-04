#pragma once
#include <string>
#include <vector>
#include <bgfx/bgfx.h>
#include <imgui.h>
#include <filesystem>
#include "ecs/Scene.h"
#include "EditorPanel.h"
#include "serialization/Serializer.h"

struct FileNode {
   std::string name;
   std::string path;
   bool isDirectory;
   std::vector<FileNode> children;
   };

class ProjectPanel : public EditorPanel {
public:
   ProjectPanel(Scene* scene);
   ~ProjectPanel() = default;

   void OnImGuiRender();
   void LoadProject(const std::string& projectPath);

   // Scene and prefab operations
   void LoadSceneFile(const std::string& filepath);
   void CreatePrefabFromEntity(EntityID entityId, const std::string& prefabPath);

private:
   void DrawFolderTree(FileNode& node);
   void DrawFileList(const std::string& folderPath);
   FileNode BuildFileTree(const std::string& path);
   
   // Helper functions for file operations
   bool IsSceneFile(const std::string& filepath) const;
   bool IsPrefabFile(const std::string& filepath) const;

private:
   std::string m_ProjectPath;
   FileNode m_ProjectRoot;
   std::string m_CurrentFolder;
   std::string m_SearchQuery;

   ImTextureID m_FolderIcon;
   ImTextureID m_FileIcon;
   };
