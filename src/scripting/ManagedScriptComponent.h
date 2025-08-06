#pragma once
#include "ScriptComponent.h"
#include "DotNetHost.h"

class ManagedScriptComponent : public ScriptComponent {
public:
   ManagedScriptComponent(const std::string& className) {
      m_Handle = CreateScriptInstance(className);
      }

   ~ManagedScriptComponent() override {
      // TODO: free handle with GCHandle::Free() equivalent if needed
      }

   void OnCreate(Entity e) override {
      CallOnCreate(m_Handle, e.GetID()); // Pass entity ID only (interop converts)
      }

   void OnUpdate(float dt) override {
      CallOnUpdate(m_Handle, dt);
      }

   std::shared_ptr<ScriptComponent> Clone() const override {
      return std::make_shared<ManagedScriptComponent>(*this);
      }


   ScriptBackend GetBackend() const override { return ScriptBackend::Managed; }

private:
   void* m_Handle = nullptr;
   };
