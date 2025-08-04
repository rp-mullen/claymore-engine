#pragma once
#include "panels/ProjectPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/ViewportPanel.h"
#include "panels/SceneHierarchyPanel.h"
#include "panels/ToolbarPanel.h"
#include "panels/MenuBarPanel.h"
#include "panels/ConsolePanel.h"
#include "ecs/Scene.h"
#include "panels/ScriptRegistryPanel.h"

#include <vector>

extern std::vector<std::string> g_RegisteredScriptNames;

class UILayer {
public:
    UILayer();
    ~UILayer() = default;

    void OnUIRender();
    void OnAttach();

    void LoadProject(std::string path);
    void ApplyStyle();

    Scene& GetScene() { return m_Scene; }

    // Camera controls (forwarded to viewport)
    void HandleCameraControls() { m_ViewportPanel.HandleCameraControls(); }

    // Picking interface (used by Application loop)
    bool HasPickRequest() const { return m_ViewportPanel.HasPickRequest(); }
    std::pair<float, float> GetNormalizedPickCoords() const { return m_ViewportPanel.GetNormalizedPickCoords(); }
    void ClearPickRequest() { m_ViewportPanel.ClearPickRequest(); }

    // Entity selection
    void SetSelectedEntity(EntityID id) { m_SelectedEntity = id; }

    void TogglePlayMode();
    
    // Deferred scene loading
    void DeferSceneLoad(const std::string& filepath);
    void ProcessDeferredSceneLoad();
    
private:
    void BeginDockspace();
    void CreateDebugCubeEntity();

    void CreateDefaultLight();

private:
    Scene m_Scene;
    EntityID m_SelectedEntity = -1;

    ProjectPanel m_ProjectPanel;
    InspectorPanel m_InspectorPanel;
    ViewportPanel m_ViewportPanel;
    SceneHierarchyPanel m_SceneHierarchyPanel;
    ToolbarPanel m_ToolbarPanel;
    MenuBarPanel m_MenuBarPanel;
    ConsolePanel m_ConsolePanel;
    ScriptRegistryPanel m_ScriptPanel;

    bool m_PlayMode = false; // Simulation state

    uint32_t m_LastViewportWidth = 0;
    uint32_t m_LastViewportHeight = 0;
    
    // Deferred scene loading
    std::string m_DeferredScenePath;
    bool m_HasDeferredSceneLoad = false;
};
