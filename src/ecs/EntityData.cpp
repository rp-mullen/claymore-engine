#include "EntityData.h"
#include "Components.h"
#include "ecs/Scene.h"

EntityData EntityData::DeepCopy(EntityID ID, Scene* newScene) const {
   EntityData copy;
   copy.Name = Name;
   copy.Transform = Transform;
   copy.Layer = Layer;
   copy.Tag = Tag;
   copy.Groups = Groups;
   copy.Visible = Visible;
   copy.Parent = Parent;
   copy.Children = Children;
   copy.EntityGuid = EntityGuid; // preserve stable identity
   copy.PrefabSource = PrefabSource;
   copy.Extra = Extra; // carry unknown fields

   // Deep copy MeshComponent
   if (Mesh)
      copy.Mesh = std::make_unique<MeshComponent>(*Mesh);

   // Deep copy Collider (already being rebuilt, but safe to copy config)
   if (Collider)
      copy.Collider = std::make_unique<ColliderComponent>(*Collider);

   // Deep copy LightComponent
   if (Light)
      copy.Light = std::make_unique<LightComponent>(*Light);

   // Deep copy CameraComponent
   if (Camera)
      copy.Camera = std::make_unique<CameraComponent>(*Camera);

   // Deep copy RigidBodyComponent
   if (RigidBody)
      copy.RigidBody = std::make_unique<RigidBodyComponent>(*RigidBody);

   // Deep copy StaticBodyComponent
   if (StaticBody)
      copy.StaticBody = std::make_unique<StaticBodyComponent>(*StaticBody);

   // Deep copy TerrainComponent
   if (Terrain)
      copy.Terrain = std::make_unique<TerrainComponent>(*Terrain);

   // Deep copy ParticleEmitterComponent
    if (Emitter)
      copy.Emitter = std::make_unique<ParticleEmitterComponent>(*Emitter);

   // Deep copy TextRendererComponent
   if (Text)
      copy.Text = std::make_unique<TextRendererComponent>(*Text);

   // Deep copy UI components
   if (Canvas)
      copy.Canvas = std::make_unique<CanvasComponent>(*Canvas);
   if (Panel)
      copy.Panel = std::make_unique<PanelComponent>(*Panel);
   if (Button)
      copy.Button = std::make_unique<ButtonComponent>(*Button);

   if (BlendShapes)
      copy.BlendShapes = std::make_unique<BlendShapeComponent>(*BlendShapes);
   if (Skeleton) {
      copy.Skeleton = std::make_unique<SkeletonComponent>();
      copy.Skeleton->InverseBindPoses = Skeleton->InverseBindPoses;
      copy.Skeleton->BoneEntities     = Skeleton->BoneEntities;
      copy.Skeleton->BoneNameToIndex  = Skeleton->BoneNameToIndex;
      copy.Skeleton->BoneParents      = Skeleton->BoneParents;
      if (Skeleton->Avatar) {
         copy.Skeleton->Avatar = std::make_unique<cm::animation::AvatarDefinition>(*Skeleton->Avatar);
      }
   }
   if (Skinning)
      copy.Skinning = std::make_unique<SkinningComponent>(*Skinning);

   // Deep copy AnimationPlayerComponent to preserve default playback setup
   if (AnimationPlayer) {
      copy.AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>(*AnimationPlayer);
   }

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