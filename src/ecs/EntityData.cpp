#include "EntityData.h"
#include "Components.h"
#include "ecs/Scene.h"

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

   // Deep copy CameraComponent
   if (Camera)
      copy.Camera = new CameraComponent(*Camera);

   // Deep copy RigidBodyComponent
   if (RigidBody)
      copy.RigidBody = new RigidBodyComponent(*RigidBody);

   // Deep copy StaticBodyComponent
   if (StaticBody)
      copy.StaticBody = new StaticBodyComponent(*StaticBody);

   // Deep copy TerrainComponent
   if (Terrain)
      copy.Terrain = new TerrainComponent(*Terrain);

   // Deep copy ParticleEmitterComponent
    if (Emitter)
      copy.Emitter = new ParticleEmitterComponent(*Emitter);

   // Deep copy TextRendererComponent
   if (Text)
      copy.Text = new TextRendererComponent(*Text);

   // Deep copy UI components
   if (Canvas)
      copy.Canvas = new CanvasComponent(*Canvas);
   if (Panel)
      copy.Panel = new PanelComponent(*Panel);
   if (Button)
      copy.Button = new ButtonComponent(*Button);

   if (BlendShapes)
      copy.BlendShapes = new BlendShapeComponent(*BlendShapes);
   if (Skeleton) {
      copy.Skeleton = new SkeletonComponent();
      copy.Skeleton->InverseBindPoses = Skeleton->InverseBindPoses;
      copy.Skeleton->BoneEntities     = Skeleton->BoneEntities;
      copy.Skeleton->BoneNameToIndex  = Skeleton->BoneNameToIndex;
      copy.Skeleton->BoneParents      = Skeleton->BoneParents;
      if (Skeleton->Avatar) {
         copy.Skeleton->Avatar = std::make_unique<cm::animation::AvatarDefinition>(*Skeleton->Avatar);
      }
   }
   if (Skinning)
      copy.Skinning = new SkinningComponent(*Skinning);

   // Deep copy AnimationPlayerComponent to preserve default playback setup
   if (AnimationPlayer) {
      copy.AnimationPlayer = new cm::animation::AnimationPlayerComponent(*AnimationPlayer);
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