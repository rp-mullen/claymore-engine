#pragma once
#include <string>
#include <memory>
#include "ecs/Scene.h"
#include "EditorPanel.h"
#include "ui/panels/ViewportPanel.h"
#include "ui/panels/SceneHierarchyPanel.h"
#include "ui/panels/InspectorPanel.h"
#include "serialization/Serializer.h"

// Simple panel that opens a secondary viewport to edit a single prefab
class PrefabEditorPanel : public EditorPanel {
public:
    explicit PrefabEditorPanel(const std::string& prefabPath, class UILayer* uiLayer);
    ~PrefabEditorPanel() = default;

    // Draws ImGui window(s). Returns false when the panel is closed by the user.
    void OnImGuiRender();

    bool IsOpen() const { return m_IsOpen; }
private:
    // Helper to load prefab file into the internal scene
    void LoadPrefab(const std::string& path);

private:
    std::string m_PrefabPath;
    bool m_IsOpen = true;
    bool m_Docked = false;

    Scene m_Scene;
    EntityID m_SelectedEntity = -1;

    // Sub-panels for editing
    ViewportPanel m_ViewportPanel;
    SceneHierarchyPanel m_HierarchyPanel;
    InspectorPanel m_InspectorPanel;

    class UILayer* m_UILayer;
};
