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
    // Render inspector UI without opening its own ImGui window
    void OnImGuiRenderEmbedded();
    // External selection hook: when a project asset (e.g., scene file) is selected
    void SetSelectedAssetPath(const std::string& path) { m_SelectedAssetPath = path; }
   void SetAvatarBuilderPanel(AvatarBuilderPanel* panel) { m_AvatarBuilder = panel; }
   // Allow switching the selected entity pointer at runtime (to follow active editor scene)
   void SetSelectedEntityPtr(EntityID* ptr) { m_SelectedEntity = ptr; }
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
    void DrawInspectorContents();
    void DrawGroupingControls(EntityID entity);
    void DrawSceneFilePreview();

private:
   EntityID* m_SelectedEntity = nullptr;
   bool m_ShowAddComponentPopup = false;
    bool m_HasAnimatorBinding = false;
    AnimatorStateBinding m_AnimatorBinding;
   AvatarBuilderPanel* m_AvatarBuilder = nullptr;
    // Rename state for entity name in inspector
    bool m_RenamingEntityName = false;
    char m_RenameBuffer[128] = {0};
    std::string m_SelectedAssetPath;
   };
