#pragma once
#include <string>
#include <vector>
#include "Entity.h"
#include "Components.h"
#include "scripting/ScriptSystem.h"
#include <scripting/ScriptComponent.h>

// Forward declaration
class Scene;

constexpr EntityID INVALID_ENTITY_ID = static_cast<EntityID>(-1);

struct EntityData {
   std::string Name = "Entity";

   TransformComponent Transform;
   MeshComponent* Mesh = nullptr; // Optional Mesh
   LightComponent* Light = nullptr; // Optional Light
   BlendShapeComponent* BlendShapes = nullptr; // Optional BlendShapes
   SkeletonComponent* Skeleton = nullptr; // Optional Skeleton
   SkinningComponent* Skinning = nullptr; // Optional Skinning
   ColliderComponent* Collider = nullptr; // Optional Collider
   CameraComponent* Camera = nullptr; // Optional Camera
   RigidBodyComponent* RigidBody = nullptr; // Optional RigidBody
   StaticBodyComponent* StaticBody = nullptr; // Optional StaticBody

   std::vector<ScriptInstance> Scripts;

   int Layer = 0;
   std::string Tag;

   EntityID Parent = INVALID_ENTITY_ID;
   std::vector<EntityID> Children;

   
   /// ----------------------------------------------------------------------
   /// <summary>
   ///  DEEP COPY: Creates a new pointer and copies all data to it.
   /// This way, the original state of the copied object remains unchanged
   /// after Play mode is exited.
   /// </summary>
   /// <param name="ID"></param>
   /// <param name="newScene"></param>
   /// <returns>EntityData returns the data for the copied entity</returns>
   /// ----------------------------------------------------------------------
   EntityData DeepCopy(EntityID ID, Scene* newScene) const;
};