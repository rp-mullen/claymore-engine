#pragma once
#include <imgui.h>
#include "ecs/Scene.h"
#define NOMINMAX

#include <windows.h>      // Base Windows API
#include <shobjidl.h> 
#include "ProjectPanel.h"
#include "EditorPanel.h"

class MenuBarPanel : public EditorPanel{
public:
    MenuBarPanel(Scene* scene, EntityID* selectedEntity, ProjectPanel* projectPanel)
        : m_SelectedEntity(selectedEntity), m_ProjectPanel(projectPanel) {
       SetContext(scene);
       }



   void OnImGuiRender();

private:
   EntityID* m_SelectedEntity;

   ProjectPanel* m_ProjectPanel;
   };
