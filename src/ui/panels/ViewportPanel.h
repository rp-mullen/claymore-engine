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

    ViewportPanel(Scene& scene, EntityID* selectedEntity)
        : m_SelectedEntity(selectedEntity) {
       SetContext(&scene);
       // Create toolbar associated with this viewport
       m_Toolbar = std::make_unique<ViewportToolbar>(this);
       }

    void OnImGuiRender(bgfx::TextureHandle sceneTexture);
    void HandleCameraControls();

    // Gizmo operation control
    void SetOperation(ImGuizmo::OPERATION op) { m_CurrentOperation = op; }
    ImGuizmo::OPERATION GetCurrentOperation() const { return m_CurrentOperation; }

    // Picking API
    bool HasPickRequest() const { return m_ShouldPick; }
    std::pair<float, float> GetNormalizedPickCoords() const {
        return { m_NormalizedPickX, m_NormalizedPickY };
    }
    void ClearPickRequest() { m_ShouldPick = false; }

private:
    void HandleEntityPicking();
    void Draw2DGrid();
    void HandleAssetDragDrop(const ImVec2& viewportPos);
    void DrawGizmo();
    void UpdateGhostPosition(float mouseX, float mouseY);
    void DrawGhostPreview();
    void DecomposeMatrix(const float* matrix, glm::vec3& pos, glm::vec3& rot, glm::vec3& scale);

    void FinalizeAssetDrop();

private:
    ImVec2 m_ViewportSize = { 0, 0 }; // actual drawn viewport image size (letterboxed)
    ImVec2 m_ViewportPos = { 0, 0 };  // screen-space top-left of the viewport image
    bool m_ShouldPick = false;

    // Mouse coords for picking (normalized)
    float m_NormalizedPickX = 0.0f;
    float m_NormalizedPickY = 0.0f;

    // Orbit camera state
    float m_Yaw = 0.0f, m_Pitch = 0.0f, m_Distance = 10.0f;
    glm::vec3 m_Target = glm::vec3(0.0f);

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
};
