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
    // Query whether this editor window is the active target; used to switch hierarchy context
    bool IsWindowFocusedOrHovered() const { return m_IsFocusedOrHovered; }
    // Accessors to expose scene/selection so UILayer can point panels at this editor when active
    Scene* GetScene() { return &m_Scene; }
    EntityID* GetSelectedEntityPtr() { return &m_SelectedEntity; }
private:
    // Helper to load prefab file into the internal scene
    void LoadPrefab(const std::string& path);

private:
    std::string m_PrefabPath;
    bool m_IsOpen = true;
    bool m_Docked = false;
    mutable bool m_IsFocusedOrHovered = false;

    Scene m_Scene;
    EntityID m_SelectedEntity = -1;

    // Sub-panels for editing
    ViewportPanel m_ViewportPanel;
    SceneHierarchyPanel m_HierarchyPanel;
    InspectorPanel m_InspectorPanel;

    class UILayer* m_UILayer;
};
