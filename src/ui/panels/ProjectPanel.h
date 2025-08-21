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

class UILayer; // Forward declaration

class ProjectPanel : public EditorPanel {
public:
   ProjectPanel(Scene* scene, UILayer* uiLayer);
   ~ProjectPanel() = default;

   void OnImGuiRender();
   void LoadProject(const std::string& projectPath);

    const std::string& GetSelectedItemName() const { return m_SelectedItemName; }
    const std::string& GetSelectedItemPath() const { return m_SelectedItemPath; }
    std::string GetSelectedItemExtension() const {
        if (m_SelectedItemPath.empty()) return std::string();
        std::string ext = std::filesystem::path(m_SelectedItemPath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext;
    }

   // Scene and prefab operations
   void LoadSceneFile(const std::string& filepath);
   void CreatePrefabFromEntity(EntityID entityId, const std::string& prefabPath);

private:
   void DrawFolderTree(FileNode& node);
   void DrawFileList(const std::string& folderPath);
   void CreateMaterialAt(const std::string& materialPath, const std::string& shaderPath);
   void DrawSelectedInspector();
   void DrawScenePreviewInspector(const std::string& scenePath);
   FileNode BuildFileTree(const std::string& path);
   
   // Helper functions for file operations
   bool IsSceneFile(const std::string& filepath) const;
   bool IsPrefabFile(const std::string& filepath) const;
    ImTextureID GetFileIconForPath(const std::string& path) const;
    void EnsureExtraIconsLoaded() const;

private:
   std::string m_ProjectPath;
   FileNode m_ProjectRoot;
   std::string m_CurrentFolder;
   std::string m_SearchQuery;
    std::string m_SelectedItemName;
    std::string m_SelectedItemPath;

   ImTextureID m_FolderIcon;
   ImTextureID m_FileIcon;
    // Additional icons
    mutable bool m_IconsLoaded = false;
    mutable ImTextureID m_Icon3DModel{};
    mutable ImTextureID m_IconImage{};
    mutable ImTextureID m_IconMaterial{};
    mutable ImTextureID m_IconScene{};
    mutable ImTextureID m_IconPrefab{};
    mutable ImTextureID m_IconAnimation{};
    mutable ImTextureID m_IconCSharp{};
    mutable ImTextureID m_IconAnimController{};

   UILayer* m_UILayer = nullptr; // Non-owning pointer back to UI layer
   };
