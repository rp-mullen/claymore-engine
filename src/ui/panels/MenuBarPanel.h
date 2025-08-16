#pragma once
#include <imgui.h>
#include "ecs/Scene.h"
#define NOMINMAX

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>      // Base Windows API
#include <shobjidl.h> 
#include "ProjectPanel.h"
#include "EditorPanel.h"
#include <string>

class UILayer; // Forward declaration 

class MenuBarPanel : public EditorPanel{
public:
    MenuBarPanel(Scene* scene, EntityID* selectedEntity, ProjectPanel* projectPanel, UILayer* uiLayer = nullptr)
        : m_SelectedEntity(selectedEntity), m_ProjectPanel(projectPanel), m_UILayer(uiLayer) {
       SetContext(scene);
       }



   void OnImGuiRender();
   void RenderExportPopup();

private:
   EntityID* m_SelectedEntity;

   ProjectPanel* m_ProjectPanel;
   UILayer* m_UILayer;

   // Export dialog state
   bool m_ExportPopupOpen = false;
   std::string m_ExportOutDir;
   std::string m_ExportEntryScene;
   bool m_ExportIncludeAll = false;
   };
