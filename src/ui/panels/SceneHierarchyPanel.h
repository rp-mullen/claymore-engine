#pragma once
#include <imgui.h>
#include "ecs/Scene.h"
#include "EditorPanel.h"

class SceneHierarchyPanel : public EditorPanel {
public:
   SceneHierarchyPanel(Scene* scene, EntityID* selectedEntity);
   ~SceneHierarchyPanel() = default;

   void OnImGuiRender();
    // Allow switching the selected entity pointer at runtime (to follow active scene)
    void SetSelectedEntityPtr(EntityID* ptr) { m_SelectedEntity = ptr; }

private:

   void DrawEntityNode(const Entity& entity);
   EntityID* m_SelectedEntity;
    // Icons for visibility toggles
    void EnsureIconsLoaded();
    bool m_IconsLoaded = false;
    ImTextureID m_VisibleIcon{};
    ImTextureID m_NotVisibleIcon{};
    // Rename state
    EntityID m_RenamingEntity = -1;
    char m_RenameBuffer[128] = {0};
   };
