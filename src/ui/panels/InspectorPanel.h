#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include "ecs/Scene.h"
#include "EditorPanel.h"
#include "scripting/ScriptReflection.h"

extern std::vector<std::string> g_RegisteredScriptNames;

class InspectorPanel : public EditorPanel {
public:
   InspectorPanel(Scene* scene, EntityID* selectedEntity)
      : m_SelectedEntity(selectedEntity) {
      SetContext(scene);
      }


   void OnImGuiRender();

private:
   void DrawComponents(EntityID entity);
   void DrawAddComponentButton(EntityID entity);
   void DrawScriptComponent(const ScriptInstance& script, int index, EntityID entity);
   void DrawScriptProperty(PropertyInfo& property, void* scriptHandle);

private:
   EntityID* m_SelectedEntity = nullptr;
   bool m_ShowAddComponentPopup = false;
   };
