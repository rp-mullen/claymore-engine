#pragma once
#include "ScriptComponent.h"
#include "ManagedScriptComponent.h"
#include <functional>

class ScriptSystem {
public:
   using ScriptFactory = std::function<std::shared_ptr<ScriptComponent>()>;

   static ScriptSystem& Instance() {
      static ScriptSystem instance;
      return instance;
      }

   void Register(const std::string& className, ScriptFactory factory) {
      m_Factories[className] = factory;
      }

   void RegisterManaged(const std::string& className) {
      m_Factories[className] = [className]() {
         return std::make_shared<ManagedScriptComponent>(className);
         };
      }

   const std::unordered_map<std::string, ScriptFactory>& GetRegistry() const {
      return m_Factories;
      }


   std::shared_ptr<ScriptComponent> Create(const std::string& className) {
      auto it = m_Factories.find(className);
      if (it != m_Factories.end())
         return it->second();

      // If not native, assume managed
      return std::make_shared<ManagedScriptComponent>(className);
      }


private:
   std::unordered_map<std::string, ScriptFactory> m_Factories;
   };
