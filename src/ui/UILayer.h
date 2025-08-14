#pragma once
#include "panels/ProjectPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/ViewportPanel.h"
#include "panels/SceneHierarchyPanel.h"
#include "panels/ToolbarPanel.h"
#include "panels/MenuBarPanel.h"
#include "panels/ConsolePanel.h"
#include "panels/PrefabEditorPanel.h"
#include "panels/AnimationControllerPanel.h"
// Legacy timeline panel removed in favor of new editor/animation panel
#include "editor/animation/AnimationTimelinePanel.h"
#include "panels/AvatarBuilderPanel.h"
#include "ecs/Scene.h"
#include "panels/ScriptRegistryPanel.h"

#include <vector>
#include <memory>

extern std::vector<std::string> g_RegisteredScriptNames;

class UILayer {
public:
    UILayer();
    ~UILayer();

    void OnUIRender();
    void OnAttach();

    void LoadProject(std::string path);
    void ApplyStyle();
    void RequestLayoutReset();

    Scene& GetScene() { return m_Scene; }
    class AnimationInspectorPanel* GetAnimationInspector() { return m_AnimationInspector.get(); }

    // Camera controls (forwarded to viewport)
    void HandleCameraControls() { m_ViewportPanel.HandleCameraControls(); }

    // Picking interface (used by Application loop)
    bool HasPickRequest() const { return m_ViewportPanel.HasPickRequest(); }
    std::pair<float, float> GetNormalizedPickCoords() const { return m_ViewportPanel.GetNormalizedPickCoords(); }
    void ClearPickRequest() { m_ViewportPanel.ClearPickRequest(); }

    // Entity selection
    void SetSelectedEntity(EntityID id) { m_SelectedEntity = id; }
    EntityID GetSelectedEntity() const { return m_SelectedEntity; }

    void TogglePlayMode();

    // Prefab editor management
    void OpenPrefabEditor(const std::string& prefabPath);
    bool AnyPrefabViewportFocused() const;
    // Access to Project panel
    ProjectPanel& GetProjectPanel() { return m_ProjectPanel; }
    class AnimTimelinePanel& GetTimelinePanel() { return m_AnimTimelinePanel; }
    
    // Deferred scene loading
    void DeferSceneLoad(const std::string& filepath);
    void ProcessDeferredSceneLoad();
    void SetCurrentScenePath(const std::string& path) { m_CurrentScenePath = path; }
    
public:
    void FocusConsoleNextFrame() { m_FocusConsoleNextFrame = true; }

private:
    void BeginDockspace();
    void CreateDebugCubeEntity();

    void CreateDefaultLight();

public:
    ImGuiID GetMainDockspaceID() const { return m_MainDockspaceID; }

private:
    Scene m_Scene;
    std::unique_ptr<class AnimationInspectorPanel> m_AnimationInspector;
    EntityID m_SelectedEntity = -1;
    EntityID m_PreviousSelectedEntity = -1;

    ProjectPanel m_ProjectPanel;
    InspectorPanel m_InspectorPanel;
    ViewportPanel m_ViewportPanel;
    SceneHierarchyPanel m_SceneHierarchyPanel;
    ToolbarPanel m_ToolbarPanel;
    MenuBarPanel m_MenuBarPanel;
    ConsolePanel m_ConsolePanel;
    ScriptRegistryPanel m_ScriptPanel;
    AnimationControllerPanel m_AnimCtrlPanel;
    class AnimTimelinePanel m_AnimTimelinePanel{};
    AvatarBuilderPanel m_AvatarBuilderPanel{ &m_Scene };

    bool m_PlayMode = false; // Simulation state
    bool m_FocusConsoleNextFrame = false;
    bool m_FocusViewportNextFrame = false;

    uint32_t m_LastViewportWidth = 0;
    ImGuiID m_MainDockspaceID = 0;
    bool m_LayoutInitialized = false;
    bool m_ResetLayoutRequested = false;

    // Active prefab editors
    std::vector<std::unique_ptr<PrefabEditorPanel>> m_PrefabEditors;
    uint32_t m_LastViewportHeight = 0;
    
    // Deferred scene loading
    std::string m_DeferredScenePath;
    bool m_HasDeferredSceneLoad = false;

    // Scene file path tracking for Ctrl+S
    std::string m_CurrentScenePath;

    // Sticky routing for shared hierarchy/inspector to the currently active editor source
    Scene* m_ActiveEditorScene = nullptr;
    EntityID* m_ActiveSelectedEntityPtr = nullptr;
};
