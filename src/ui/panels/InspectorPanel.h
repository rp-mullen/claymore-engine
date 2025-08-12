#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include "ecs/Scene.h"
#include "EditorPanel.h"
// Legacy timeline panel removed
#include "scripting/ScriptReflection.h"

class AvatarBuilderPanel; // forward decl

extern std::vector<std::string> g_RegisteredScriptNames;

class InspectorPanel : public EditorPanel {
public:
   InspectorPanel(Scene* scene, EntityID* selectedEntity)
      : m_SelectedEntity(selectedEntity) {
      SetContext(scene);
      }


   void OnImGuiRender();
   void SetAvatarBuilderPanel(AvatarBuilderPanel* panel) { m_AvatarBuilder = panel; }
   // Animator node selection bridge
   void ShowAnimatorStateProperties(const std::string& stateName,
                                    std::string& clipPath,
                                    float& speed,
                                    bool& loop,
                                    bool isDefault,
                                    std::function<void()> onMakeDefault,
                                    std::vector<std::pair<std::string, int>>* conditionsInt = nullptr,
                                    std::vector<std::tuple<std::string, int, float>>* conditionsFloat = nullptr);

   struct AnimatorStateBinding {
       std::string* Name = nullptr;
       std::string* ClipPath = nullptr;
       std::string* AssetPath = nullptr;
       float* Speed = nullptr;
       bool* Loop = nullptr;
       bool IsDefault = false;
       std::function<void()> MakeDefault;
   };
   void SetAnimatorStateBinding(const AnimatorStateBinding& binding) { m_AnimatorBinding = binding; m_HasAnimatorBinding = true; }
   void ClearAnimatorBinding() { m_HasAnimatorBinding = false; m_AnimatorBinding = {}; }

private:
   void DrawComponents(EntityID entity);
   void DrawAddComponentButton(EntityID entity);
   void DrawScriptComponent(const ScriptInstance& script, int index, EntityID entity);
   void DrawScriptProperty(PropertyInfo& property, void* scriptHandle);

private:
   EntityID* m_SelectedEntity = nullptr;
   bool m_ShowAddComponentPopup = false;
    bool m_HasAnimatorBinding = false;
    AnimatorStateBinding m_AnimatorBinding;
   AvatarBuilderPanel* m_AvatarBuilder = nullptr;
   };
