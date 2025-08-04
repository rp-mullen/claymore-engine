#pragma once
#include <cstdint>
#include <string>

class Scene;

using EntityID = uint32_t;

class Entity {
public:
   Entity() : m_ID(0), m_Scene(nullptr) {}
   Entity(EntityID id) : m_ID(id), m_Scene(nullptr) {}
   Entity(EntityID id, Scene* scene) : m_ID(id), m_Scene(scene) {}

   bool IsValid() const { return m_Scene != nullptr && m_ID != 0; }
   EntityID GetID() const { return m_ID; }
   Scene* GetScene() const { return m_Scene; }

   const std::string& GetName() const;
   void SetName(const std::string& name);

private:
   EntityID m_ID;
   Scene* m_Scene;
   };
