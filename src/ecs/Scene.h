#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include "Entity.h"
#include "EntityData.h"
#include <assimp/scene.h>
#include <rendering/ModelLoader.h>
#include <rendering/Camera.h>
#include "Components.h"

class Scene {
public:
   
    static Scene* CurrentScene;
	static Scene& Get() {
		if (!CurrentScene)
			CurrentScene = new Scene();
		return *CurrentScene;
	}

    Scene() = default;

   Entity CreateEntity(const std::string& name = "Entity");
   void RemoveEntity(EntityID id);

   EntityData* GetEntityData(EntityID id);
   Entity FindEntityByID(EntityID id);

   const std::vector<Entity>& GetEntities() const { return m_EntityList; }

   Entity CreateLight(const std::string& name, LightType type, const glm::vec3& color, float intensity);

   EntityID InstantiateAsset(const std::string& path, const glm::vec3& position);
   EntityID InstantiateModel(const std::string& path, const glm::vec3& rootPosition);

   void DestroyEntity(Entity e) {RemoveEntity(e.GetID());}

   void SetParent(EntityID child, EntityID parent);
   void SetChild(EntityID parent, EntityID child) {SetParent(child, parent);}

   // Transform Updates
   void UpdateTransforms();
   void TopologicalSortEntities(std::vector<EntityID>& outSorted);
   void SetPosition(EntityID id, const glm::vec3& pos);
   void MarkTransformDirty(EntityID id);

   std::shared_ptr<Scene> RuntimeClone();

   std::shared_ptr<Scene> m_EditScene;
   std::shared_ptr<Scene> m_RuntimeScene;
   bool m_IsPlaying = false;

   std::unordered_map<EntityID, JPH::BodyID> m_BodyMap;

   void CreatePhysicsBody(EntityID id, const TransformComponent&, const ColliderComponent&);
   void DestroyPhysicsBody(EntityID id);

   void Update(float dt);

   void OnStop();

   bool HasComponent(const char* componentName);

   Camera* GetActiveCamera();

private:
   std::unordered_map<EntityID, EntityData> m_Entities;
   std::vector<Entity> m_EntityList;
   EntityID m_NextID = 1;
   };
