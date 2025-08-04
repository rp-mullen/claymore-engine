#pragma once
#include <string>
#include "Components.h"
#include "scripting/ScriptSystem.h"
#include <scripting/ScriptComponent.h>

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
   EntityData EntityData::DeepCopy(EntityID ID, Scene* newScene) const {
      EntityData copy = *this;

      // Deep copy MeshComponent
      if (Mesh)
         copy.Mesh = new MeshComponent(*Mesh);

      // Deep copy Collider (already being rebuilt, but safe to copy config)
      if (Collider)
         copy.Collider = new ColliderComponent(*Collider);

      // Deep copy LightComponent
      if (Light)
         copy.Light = new LightComponent(*Light);

      if (BlendShapes)
         copy.BlendShapes = new BlendShapeComponent(*BlendShapes);
      if (Skeleton)
         copy.Skeleton = new SkeletonComponent(*Skeleton);
      if (Skinning)
         copy.Skinning = new SkinningComponent(*Skinning);

      // Scripts: clone and rebind context
      copy.Scripts.clear();
      for (const auto& script : Scripts) {
         ScriptInstance instance;
         instance.ClassName = script.ClassName;

         // Create a new script instance using the registered factory
         auto created = ScriptSystem::Instance().Create(instance.ClassName);
         if (created) {
            instance.Instance = created;
            copy.Scripts.push_back(instance);
            }
         else {
            std::cerr << "[ScriptSystem] Failed to create script of type '" << instance.ClassName << "'\n";
            }
         }


      return copy;
      }


   };