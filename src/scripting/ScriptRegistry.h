#pragma once
#include "ScriptSystem.h"
#include <memory>

struct ScriptRegistrar {
   ScriptRegistrar(const std::string& className, ScriptSystem::ScriptFactory factory) {
      ScriptSystem::Instance().Register(className, factory);
      }
   };

#define REGISTER_SCRIPT(ClassName) \
   static ScriptRegistrar _registrar_##ClassName(#ClassName, []() -> std::shared_ptr<ScriptComponent> { \
      return std::make_shared<ClassName>(); \
   })
