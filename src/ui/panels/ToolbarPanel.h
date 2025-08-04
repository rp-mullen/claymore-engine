#pragma once
#include <string>
#include <imgui.h>
#include <bgfx/bgfx.h>

class UILayer;

enum class GizmoOperation {
    Translate,
    Rotate,
    Scale
};

class ToolbarPanel {
public:
    ToolbarPanel() = default;
    ToolbarPanel(UILayer* uiLayer)
       : m_UILayer(uiLayer) {
       }

    // Main render method for the toolbar UI
    void OnImGuiRender(ImGuiID dockspace_id);

    // Set gizmo operation
    void SetOperation(GizmoOperation op) { m_CurrentOperation = op; }
    GizmoOperation GetOperation() const { return m_CurrentOperation; }

    // Show gizmos toggle
    bool IsShowGizmosEnabled() const { return m_ShowGizmos; }

    // Play mode toggle
    void TogglePlayMode();
    bool IsPlayMode() const { return m_PlayMode; }

private:
    bool m_ShowGizmos = true;                // Default ON
    bool m_PlayMode = false;                 // Simulation play state

    UILayer* m_UILayer = nullptr; // Pointer to the main UI layer for context

    GizmoOperation m_CurrentOperation = GizmoOperation::Translate; // Default gizmo mode
};
