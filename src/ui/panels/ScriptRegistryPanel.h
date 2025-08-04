#pragma once
#include "EditorPanel.h"
#include <string>
#include <vector>

class ScriptRegistryPanel : public EditorPanel
   {
   public:
      void OnImGuiRender();

   private:
      std::vector<std::string>* m_ScriptNames = nullptr;

   public:
      void SetScriptSource(std::vector<std::string>* names)
         {
         m_ScriptNames = names;
         }
   };
