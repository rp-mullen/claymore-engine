#include "ScriptRegistryPanel.h"
#include <imgui.h>

void ScriptRegistryPanel::OnImGuiRender()
   {
   if (!m_ScriptNames) return;

   if (ImGui::Begin("Script Registry"))
      {
      ImGui::Text("Total: %d script(s)", static_cast<int>(m_ScriptNames->size()));
      ImGui::Separator();

      for (const auto& name : *m_ScriptNames)
         {
         ImGui::BulletText("%s", name.c_str());
         }
      }
   ImGui::End();
   }
 