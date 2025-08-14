#pragma once
#include <string>
#include <vector>
#include "Entity.h"
#include "Components.h"
#include "scripting/ScriptSystem.h"
#include "animation/AnimationPlayerComponent.h"
#include <scripting/ScriptComponent.h>
#include "UIComponents.h"
#include <memory>

// Forward declaration
class Scene;

constexpr EntityID INVALID_ENTITY_ID = static_cast<EntityID>(-1);

struct EntityData {
   EntityData() = default;
   EntityData(const EntityData&) = delete;
   EntityData& operator=(const EntityData&) = delete;
   EntityData(EntityData&&) = default;
   EntityData& operator=(EntityData&&) = default;
   std::string Name = "Entity";

   TransformComponent Transform;
   std::unique_ptr<MeshComponent> Mesh; // Optional Mesh
   std::unique_ptr<LightComponent> Light; // Optional Light
   std::unique_ptr<BlendShapeComponent> BlendShapes; // Optional BlendShapes
   std::unique_ptr<SkeletonComponent> Skeleton; // Optional Skeleton
   std::unique_ptr<SkinningComponent> Skinning; // Optional Skinning
   std::unique_ptr<ColliderComponent> Collider; // Optional Collider
   std::unique_ptr<CameraComponent> Camera; // Optional Camera
   std::unique_ptr<RigidBodyComponent> RigidBody; // Optional RigidBody
   std::unique_ptr<StaticBodyComponent> StaticBody; // Optional StaticBody
   std::unique_ptr<TerrainComponent> Terrain; // Optional Terrain
   std::unique_ptr<ParticleEmitterComponent> Emitter; // Optional particle emitter

   // Text rendering
   std::unique_ptr<TextRendererComponent> Text; // Optional text renderer

   // UI Components
   std::unique_ptr<CanvasComponent> Canvas; // Optional UI canvas
   std::unique_ptr<PanelComponent> Panel;   // Optional UI panel (textured rect)
   std::unique_ptr<ButtonComponent> Button; // Optional UI button behavior

   std::unique_ptr<cm::animation::AnimationPlayerComponent> AnimationPlayer; // Optional animation player

   std::vector<ScriptInstance> Scripts;

   int Layer = 0;
   std::string Tag;
    std::vector<std::string> Groups; // Arbitrary groups for filtering/searching
   // Global visibility toggle for the whole entity (affects rendering and lights)
   bool Visible = true;

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