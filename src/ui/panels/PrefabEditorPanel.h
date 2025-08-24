#pragma once
#include <string>
#include <memory>
#include "ecs/Scene.h"
#include "EditorPanel.h"
#include "ui/panels/ViewportPanel.h"
#include "ui/panels/SceneHierarchyPanel.h"
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
    // Prefab path and focus request for UILayer dedup/focus behavior
    const std::string& GetPrefabPath() const { return m_PrefabPath; }
    void RequestFocus() { m_FocusNextFrame = true; }
    bool IsDirty() const { return m_IsDirty; }
    void ClearDirty() { m_IsDirty = false; }
private:
    // Helper to load prefab file into the internal scene
    void LoadPrefab(const std::string& path);
    void EnsureEditorLighting();

private:
    std::string m_PrefabPath;
    bool m_IsOpen = true;
    bool m_Docked = false;
    mutable bool m_IsFocusedOrHovered = false;
    bool m_FocusNextFrame = false;
    bool m_IsDirty = false;

    Scene m_Scene;
    EntityID m_SelectedEntity = -1;
    EntityID m_EditorLight = -1;

    // Embedded viewport for editing
    ViewportPanel m_ViewportPanel;

    class UILayer* m_UILayer;
};
