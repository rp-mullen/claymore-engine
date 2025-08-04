#include "../Components.h"
#include "utils/Time.h"
#include "scripting/ScriptRegistry.h"

class TestScript : public ScriptComponent {
   glm::vec3 m_StartPos;
   Entity m_Entity;

public:
   void OnCreate(Entity entity) override {
      m_Entity = entity;
      auto eScene = m_Entity.GetScene();
      EntityID id = m_Entity.GetID();
      auto* data = eScene->GetEntityData(id);
      m_StartPos = data->Transform.Position;
      }

   void OnUpdate(float dt) override {
      auto eScene = m_Entity.GetScene();
      EntityID id = m_Entity.GetID();
      auto* data = eScene->GetEntityData(id);
      float offset = sinf((float)Time::GetTotalTime()) * 0.5f;
      data->Transform.Position = m_StartPos + glm::vec3(offset, 0.0f, 0.0f);
      data->Transform.TransformDirty = true;
      }

   std::shared_ptr<ScriptComponent> Clone() const override {
      return std::make_shared<TestScript>(*this); // Shallow copy works if no dynamic memory
      }
   };

REGISTER_SCRIPT(TestScript);