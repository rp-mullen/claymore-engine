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
    void SetShowGizmosEnabled(bool enabled) { m_ShowGizmos = enabled; }

    // Play mode toggle
    void TogglePlayMode();
    bool IsPlayMode() const { return m_PlayMode; }
    void TogglePause() { m_Paused = !m_Paused; }
    bool IsPaused() const { return m_Paused; }

    // Debug toggles (exposed in toolbar)
    void SetShowUIRects(bool v);
    bool GetShowUIRects() const;

private:
    void EnsureIconsLoaded();

    bool m_ShowGizmos = true;                // Default ON
    bool m_PlayMode = false;                 // Simulation play state

    UILayer* m_UILayer = nullptr; // Pointer to the main UI layer for context

    GizmoOperation m_CurrentOperation = GizmoOperation::Translate; // Default gizmo mode

    // Icon textures (zero-initialized)
    ImTextureID m_PlayIcon{};
    ImTextureID m_StopIcon{};
    ImTextureID m_PauseIcon{};
    ImTextureID m_MoveIcon{};
    ImTextureID m_RotateIcon{};
    ImTextureID m_ScaleIcon{};

    bool m_IconsLoaded = false;

    bool m_Paused = false;
};
