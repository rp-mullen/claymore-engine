#pragma once
#include <bgfx/bgfx.h>
#include <glm/glm.hpp>
#include <string>
#include <utility>
#include <imgui.h>
#include <ImGuizmo.h>
#include <memory>
#include "ecs/Entity.h"
#include "EditorPanel.h"
#include "ViewportToolbar.h"

class Scene;

class ViewportPanel : public EditorPanel {
public:

    ViewportPanel(Scene& scene, EntityID* selectedEntity, bool useInternalCamera = false)
        : m_SelectedEntity(selectedEntity), m_UseInternalCamera(useInternalCamera) {
       SetContext(&scene);
       if (m_UseInternalCamera) {
           m_Camera = std::make_unique<class Camera>(60.0f, 16.0f/9.0f, 0.1f, 100.0f);
       }
       // Create toolbar associated with this viewport
       m_Toolbar = std::make_unique<ViewportToolbar>(this);
        }

		void OnImGuiRender(bgfx::TextureHandle sceneTexture);
		// Render the viewport contents embedded inside the current ImGui window/child
		// without opening its own window. Useful for panels that host an internal viewport
		// such as the Prefab Editor.
		void OnImGuiRenderEmbedded(bgfx::TextureHandle sceneTexture, const char* idLabel = "EmbeddedViewport");
    void HandleCameraControls();
    // Editor utility: frame the currently selected entity
    void FrameSelected(float durationSeconds = 0.35f);
    // Accessor for embedded camera (nullptr when using global camera)
    class Camera* GetPanelCamera() const { return m_Camera.get(); }

    // UI: allow parent layer to set the display scene name and dirty flag for window title
    void SetDisplaySceneTitle(const std::string& title) { m_DisplaySceneTitle = title; }

    // Focus/hover state (main viewport window only)
    bool IsWindowFocusedOrHovered() const { return m_WindowFocusedOrHovered; }

    // Gizmo operation control
    void SetOperation(ImGuizmo::OPERATION op) { m_CurrentOperation = op; }
    ImGuizmo::OPERATION GetCurrentOperation() const { return m_CurrentOperation; }
    void SetShowGizmos(bool enabled) { m_ShowGizmos = enabled; }
    bool GetShowGizmos() const { return m_ShowGizmos; }

    // Picking API
    bool HasPickRequest() const { return m_ShouldPick; }
    std::pair<float, float> GetNormalizedPickCoords() const {
        return { m_NormalizedPickX, m_NormalizedPickY };
    }
    void ClearPickRequest() { m_ShouldPick = false; }

    void DrawUIRectOverlay(ImDrawList* dl, const ImVec2& viewportTL, const ImVec2& viewportSize, Scene* scene);

private:
    void HandleEntityPicking();
    void Draw2DGrid();
    void HandleAssetDragDrop(const ImVec2& viewportPos);
    void DrawGizmo();
    void DrawUIGizmo();
    void UpdateGhostPosition(float mouseX, float mouseY);
    void DrawGhostPreview();
    void DecomposeMatrix(const float* matrix, glm::vec3& pos, glm::vec3& rot, glm::vec3& scale);

    void FinalizeAssetDrop();

private:
    ImVec2 m_ViewportSize = { 0, 0 }; // actual drawn viewport image size (letterboxed)
    ImVec2 m_ViewportPos = { 0, 0 };  // screen-space top-left of the viewport image
    bool m_ShouldPick = false;
    // Track UI handle state to prevent scene picking/deselection when dragging UI elements
    bool m_UIHandleActive = false;
    bool m_UIHandleHovered = false;

    // Mouse coords for picking (normalized)
    float m_NormalizedPickX = 0.0f;
    float m_NormalizedPickY = 0.0f;

    // Orbit camera state
    float m_Yaw = 0.0f, m_Pitch = 0.0f, m_Distance = 10.0f;
    glm::vec3 m_Target = glm::vec3(0.0f);

    // Tween state for focus (editor-only)
    bool m_IsTweening = false;
    float m_TweenTime = 0.0f;
    float m_TweenDuration = 0.35f;
    float m_DistanceStart = 10.0f;
    float m_DistanceEnd = 10.0f;
    glm::vec3 m_TargetStart = glm::vec3(0.0f);
    glm::vec3 m_TargetEnd = glm::vec3(0.0f);

    bool m_ShowGizmos = true;
    bool m_IsDraggingAsset = false;
    std::string m_DraggedAssetPath;
    glm::vec3 m_GhostPosition;
    float m_GridSize = 1.0f;

    EntityID* m_SelectedEntity = nullptr;

    ImGuizmo::OPERATION m_CurrentOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_CurrentMode = ImGuizmo::WORLD;

    // Mini toolbar displayed inside the viewport
    std::unique_ptr<class ViewportToolbar> m_Toolbar;

    // Optional internal camera for fully-isolated embedded viewports
    bool m_UseInternalCamera = false;
    std::unique_ptr<class Camera> m_Camera;

    bool m_WindowFocusedOrHovered = false;

    // Window title display for main viewport (e.g., scene name + '*')
    std::string m_DisplaySceneTitle;
};
