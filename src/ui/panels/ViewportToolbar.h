#pragma once

#include <imgui.h>
#include <ImGuizmo.h>

class ViewportPanel;

// Small stacked toolbar inside the Viewport window that lets the user
// switch between Translate / Rotate / Scale gizmo operations. Designed
// to mimic the layout of the Unity editor.
class ViewportToolbar {
public:
    explicit ViewportToolbar(ViewportPanel* viewport)
        : m_Viewport(viewport) {}

    // Render the toolbar; must be called while the Viewport window is
    // current (inside the same ImGui window scope).
    void OnImGuiRender();

private:
    ViewportPanel* m_Viewport = nullptr; // not owned
};
