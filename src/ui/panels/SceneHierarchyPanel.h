#pragma once
#include <imgui.h>
#include "ecs/Scene.h"
#include "EditorPanel.h"

class SceneHierarchyPanel : public EditorPanel {
public:
   SceneHierarchyPanel(Scene* scene, EntityID* selectedEntity);
   ~SceneHierarchyPanel() = default;

   void OnImGuiRender();

private:

   void DrawEntityNode(const Entity& entity);
   EntityID* m_SelectedEntity;
   };
