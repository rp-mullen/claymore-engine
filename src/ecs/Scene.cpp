#include "Scene.h"
#include "EntityData.h"
#include <algorithm>
#include <functional>
#include <filesystem>
namespace fs = std::filesystem;
#include <rendering/ModelLoader.h>
#include <rendering/StandardMeshManager.h>
#include "ecs/AnimationComponents.h"
#include <rendering/TextureLoader.h>
#include <rendering/MaterialManager.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>


#include <sstream> // Include for std::ostringstream
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

Scene* Scene::CurrentScene = nullptr;

Entity Scene::CreateEntity(const std::string& name) {
   EntityID id = m_NextID++;
   EntityData data;

   // Convert id to string and concatenate with name
   std::ostringstream oss;
   oss << name << "_" << id;
   data.Name = oss.str();

   m_Entities[id] = data;

   Entity entity(id, this);
   m_EntityList.push_back(entity);

   return entity;
}

void Scene::RemoveEntity(EntityID id) {
    auto* data = GetEntityData(id);
    if (!data) return;

    // 1. Clean up parent-child relationships
    if (data->Parent != INVALID_ENTITY_ID) {
        auto* parentData = GetEntityData(data->Parent);
        if (parentData) {
            parentData->Children.erase(
                std::remove(parentData->Children.begin(), parentData->Children.end(), id),
                parentData->Children.end()
            );
        }
    }

    // 2. Clean up children (recursively remove all children)
    // Make a copy to avoid issues with iterating while removing
    std::vector<EntityID> childrenToRemove = data->Children;
    for (EntityID childID : childrenToRemove) {
        RemoveEntity(childID);
    }

    // 3. Clean up physics body
    DestroyPhysicsBody(id);

    // 4. Clean up allocated components
    if (data->Mesh) {
        delete data->Mesh;
        data->Mesh = nullptr;
    }
    if (data->Light) {
        delete data->Light;
        data->Light = nullptr;
    }
    if (data->Collider) {
        delete data->Collider;
        data->Collider = nullptr;
    }
    if (data->Camera) {
        delete data->Camera;
        data->Camera = nullptr;
    }
    if (data->RigidBody) {
        delete data->RigidBody;
        data->RigidBody = nullptr;
    }
    if (data->StaticBody) {
        delete data->StaticBody;
        data->StaticBody = nullptr;
    }
    if (data->BlendShapes) {
        delete data->BlendShapes;
        data->BlendShapes = nullptr;
    }
    if (data->Skeleton) {
        delete data->Skeleton;
        data->Skeleton = nullptr;
    }
    if (data->Skinning) {
        delete data->Skinning;
        data->Skinning = nullptr;
    }

    // 5. Clean up scripts
    for (auto& script : data->Scripts) {
        // Note: Script_Destroy is not exposed to C++, so we just clear the vector
        // The shared_ptr will handle cleanup of native script instances
        // For managed scripts, the GCHandle cleanup happens in C# when the script is destroyed
    }
    data->Scripts.clear();

    // 6. Remove from entity collections
    m_Entities.erase(id);
    m_EntityList.erase(
        std::remove_if(m_EntityList.begin(), m_EntityList.end(),
            [&](const Entity& e) { return e.GetID() == id; }), 
        m_EntityList.end()
    );

    std::cout << "[Scene] Removed entity " << id << " and all its children" << std::endl;
}

EntityData* Scene::GetEntityData(EntityID id) {
    auto it = m_Entities.find(id);
    return (it != m_Entities.end()) ? &it->second : nullptr;
}

Entity Scene::FindEntityByID(EntityID id) {
    auto it = std::find_if(m_EntityList.begin(), m_EntityList.end(),
        [&](const Entity& e) { return e.GetID() == id; });
    return (it != m_EntityList.end()) ? *it : Entity();
}

Entity Scene::CreateLight(const std::string& name, LightType type, const glm::vec3& color, float intensity) {
   Entity entity = CreateEntity(name);
   if (auto* data = GetEntityData(entity.GetID())) {
      data->Light = new LightComponent{ type, color, intensity };
      }
   return entity;
   }


EntityID Scene::InstantiateAsset(const std::string& path, const glm::vec3& position) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf") {
       return InstantiateModel(path, position);
       }
   else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
      // Create a simple textured quad
      Entity entity = CreateEntity("ImageQuad");
      auto* data = GetEntityData(entity.GetID());
      if (!data) return -1;

      data->Transform.Position = glm::vec3(0.0f);
      data->Transform.Rotation = glm::vec3(0.0f);
      data->Transform.Scale = glm::vec3(1.0f);

      std::shared_ptr<Mesh> quadMesh = StandardMeshManager::Instance().GetPlaneMesh();

      bgfx::TextureHandle tex = TextureLoader::Load2D(path);

      data->Mesh = new MeshComponent();
      data->Mesh->mesh = quadMesh;

      data->Mesh->MeshName = "ImageQuad";
      data->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();

      // TODO: if you want to store `tex`, create a TextureComponent or assign it in material.

      return entity.GetID();
      }
   else {
      std::cerr << "[Scene] Unsupported asset type: " << ext << std::endl;
      return -1;
      }
   }

EntityID Scene::InstantiateModel(const std::string& path, const glm::vec3& rootPosition) {
    Assimp::Importer importer;
    const aiScene* aScene = importer.ReadFile(path,
       aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices | 
       aiProcess_ConvertToLeftHanded | aiProcess_FixInfacingNormals | aiProcess_GlobalScale);

    if (!aScene || !aScene->mRootNode) {
        std::cerr << "[Scene] Failed to load model: " << path << std::endl;
        return -1;
    }

    // Load meshes and placeholder materials once
    Model model = ModelLoader::LoadModel(path);

    // Root entity that encapsulates the whole model
    Entity rootEntity = CreateEntity("ImportedModel");
    EntityID rootID = rootEntity.GetID();
    auto* rootData = GetEntityData(rootID);
    if (!rootData) return -1;
    rootData->Transform.Position = rootPosition;
    rootData->Transform.Rotation = glm::vec3(0.0f);
    const float importScale = 1.f;
    rootData->Transform.Scale    = glm::vec3(importScale);
    rootData->Transform.TransformDirty = true;

    //--------------------------------------------------------------------
    // Build map of meshIndex -> transform relative to the FBX root
    //--------------------------------------------------------------------
    std::vector<glm::mat4> meshTransforms(aScene->mNumMeshes, glm::mat4(1.0f));

    // Helper: Assimp (row-major) -> GLM (column-major)
    auto AiToGlm = [](const aiMatrix4x4& m) {
        glm::mat4 mat(
            m.a1, m.b1, m.c1, m.d1,
            m.a2, m.b2, m.c2, m.d2,
            m.a3, m.b3, m.c3, m.d3,
            m.a4, m.b4, m.c4, m.d4);
        return glm::transpose(mat);
    };

    glm::mat4 rootLocal   = AiToGlm(aScene->mRootNode->mTransformation);
    glm::mat4 invRoot     = glm::inverse(rootLocal);

    // Recursive lambda to accumulate transforms
    std::function<void(aiNode*, const glm::mat4&)> traverse;
    traverse = [&](aiNode* node, const glm::mat4& parentTransform) {
        glm::mat4 local  = AiToGlm(node->mTransformation);
        glm::mat4 global = parentTransform * local;
        glm::mat4 relative = invRoot * global; // make it relative to model root

        // Store relative transform for all meshes referenced by this node
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            unsigned int meshIndex = node->mMeshes[i];
            if (meshIndex < meshTransforms.size())
                meshTransforms[meshIndex] = relative;
        }
        // Recurse into children
        for (unsigned int c = 0; c < node->mNumChildren; ++c) {
            traverse(node->mChildren[c], global);
        }
    };
    traverse(aScene->mRootNode, glm::mat4(1.0f));

    //--------------------------------------------------------------------
    // ---------------- Skeleton creation ----------------
    EntityID skeletonRootID = -1;
    if (!model.BoneNames.empty()) {
        Entity skeletonRootEnt = CreateEntity("SkeletonRoot");
        skeletonRootID = skeletonRootEnt.GetID();
        SetParent(skeletonRootID, rootID);

        auto* skelData = GetEntityData(skeletonRootID);
        skelData->Skeleton = new SkeletonComponent();
        skelData->Skeleton->InverseBindPoses = model.InverseBindPoses;

        // Create bone entities flat under skeleton root for now
        for (size_t b = 0; b < model.BoneNames.size(); ++b) {
            Entity boneEnt = CreateEntity(model.BoneNames[b]);
            EntityID boneID = boneEnt.GetID();
            SetParent(boneID, skeletonRootID);
            skelData->Skeleton->BoneEntities.push_back(boneID);
        }
    }



    //--------------------------------------------------------------------
    // Create one entity per mesh as child of the root entity
    //--------------------------------------------------------------------
    for (size_t i = 0; i < model.Meshes.size(); ++i) {
        const auto& meshPtr = model.Meshes[i];
        if (!meshPtr) continue;

        Entity meshEntity = CreateEntity("Mesh_" + std::to_string(i));
        EntityID meshID = meshEntity.GetID();
        SetParent(meshID, rootID);

        auto* meshData = GetEntityData(meshID);
        if (!meshData) continue;

        // Decompose transform
        glm::vec3 translation, scale, skew;
        glm::vec4 perspective;
        glm::quat rotationQuat;
        glm::decompose(meshTransforms[i], scale, rotationQuat, translation, skew, perspective);
        glm::vec3 rotationEuler = glm::degrees(glm::eulerAngles(rotationQuat));

        meshData->Transform.Position = translation;
        meshData->Transform.Rotation = rotationEuler;
                // Clamp unreasonable global scaling from FBX (e.g., 100)
        if (scale.x > 50.0f || scale.y > 50.0f || scale.z > 50.0f)
            scale = glm::vec3(1.0f);
        meshData->Transform.Scale    = scale;
        meshData->Transform.TransformDirty = true;

        // Attach mesh component
        auto mat = (i < model.Materials.size() && model.Materials[i]) ? model.Materials[i]
                    : MaterialManager::Instance().CreateDefaultPBRMaterial();
        meshData->Mesh = new MeshComponent(meshPtr, "Mesh_" + std::to_string(i), mat);
        if (meshPtr->HasSkinning()) {
            meshData->Skinning = new SkinningComponent();
            meshData->Skinning->SkeletonRoot = skeletonRootID;
            meshData->Skinning->Palette.resize(model.BoneNames.size(), glm::mat4(1.0f));
        }

        // Blend shapes
        if (i < model.BlendShapes.size() && !model.BlendShapes[i].Shapes.empty()) {
            auto bsPtr = new BlendShapeComponent(model.BlendShapes[i]);
            meshData->BlendShapes = bsPtr;
            meshData->Mesh->BlendShapes = bsPtr;
        }
    }

    std::cout << "[Scene] Imported model with " << model.Meshes.size() << " mesh entities under root " << rootID << std::endl;
    return rootID;
}




void Scene::SetParent(EntityID child, EntityID parent) {
   auto* childData = GetEntityData(child);
   auto* parentData = GetEntityData(parent);
   if (!childData || !parentData) return;

   if (childData->Parent != -1) {
      auto* oldParent = GetEntityData(childData->Parent);
      if (oldParent)
         oldParent->Children.erase(std::remove(oldParent->Children.begin(), oldParent->Children.end(), child),
            oldParent->Children.end());
      }

   childData->Parent = parent;
   parentData->Children.push_back(child);
   }


void Scene::UpdateTransforms() {
   std::vector<EntityID> sorted;
   TopologicalSortEntities(sorted); // Parents before children

   for (EntityID id : sorted) {
      auto* data = GetEntityData(id);
      if (!data) continue;

      bool parentDirty = false;
      glm::mat4 parentWorld = glm::mat4(1.0f);

      if (data->Parent != -1) {
         EntityData* parent = GetEntityData(data->Parent);
         parentDirty = parent->Transform.TransformDirty;
         parentWorld = parent->Transform.WorldMatrix;
         }

      if (data->Transform.TransformDirty || parentDirty) {
         data->Transform.CalculateLocalMatrix();
         data->Transform.WorldMatrix = parentWorld * data->Transform.LocalMatrix;
         data->Transform.TransformDirty = false;
         
         // Debug: Print transform updates for parent-child relationships
         if (data->Parent != -1) {
            std::cout << "[Transform] Updated Entity " << id << " (Parent: " << data->Parent 
                      << ") - Local Pos: (" << data->Transform.Position.x << ", " 
                      << data->Transform.Position.y << ", " << data->Transform.Position.z << ")"
                      << " - World Pos: (" << data->Transform.WorldMatrix[3].x << ", "
                      << data->Transform.WorldMatrix[3].y << ", " << data->Transform.WorldMatrix[3].z << ")" << std::endl;
         }
         }
      }
   }



void Scene::TopologicalSortEntities(std::vector<EntityID>& outSorted) {
   std::unordered_set<EntityID> visited;

   std::function<void(EntityID)> visit = [&](EntityID id) {
      if (visited.count(id)) return;
      visited.insert(id);

      EntityData* data = GetEntityData(id);
      if (data && data->Children.size()) {
         for (EntityID child : data->Children) {
            visit(child);
            }
         }

      outSorted.push_back(id);
      };

   for (const Entity& e : m_EntityList) {
      EntityID id = e.GetID();
      EntityData* data = GetEntityData(id);
      if (data && data->Parent == -1)
         visit(id);
      }

   std::reverse(outSorted.begin(), outSorted.end()); // root-first
   }

void Scene::SetPosition(EntityID id, const glm::vec3& pos) {
   auto* data = GetEntityData(id);
   if (data) {
      data->Transform.Position = pos;
      MarkTransformDirty(id);
      }
   }

void Scene::MarkTransformDirty(EntityID id) {
   auto* data = GetEntityData(id);
   if (!data || data->Transform.TransformDirty) return;
   data->Transform.TransformDirty = true;

   for (EntityID child : data->Children)
      MarkTransformDirty(child);
   }


// --------------------------------------------------------
// Create a clone of the current scene for play mode.
// This will copy entities, their data, and scripts.
// --------------------------------------------------------
std::shared_ptr<Scene> Scene::RuntimeClone() {
   auto clone = std::make_shared<Scene>();
   std::vector<std::pair<ScriptInstance*, Entity>> toInitialize;

   clone->m_Entities.clear();
   clone->m_EntityList.clear();
   clone->m_BodyMap.clear();
   clone->m_NextID = m_NextID;

   // Clone entities
   for (const Entity& e : m_EntityList) {
      EntityID id = e.GetID();

      clone->m_EntityList.emplace_back(id, clone.get());
      clone->m_Entities[id] = m_Entities.at(id).DeepCopy(id, clone.get());
      
      auto& data = clone->m_Entities[id];

      // Mark transform as dirty so world matrices are computed
      data.Transform.TransformDirty = true;

      for (auto& script : data.Scripts) {
         if (script.Instance)
            toInitialize.emplace_back(&script, Entity(id, clone.get()));
         }
      }

   // Initialize transforms for the cloned scene BEFORE creating physics bodies
   clone->UpdateTransforms();

   // Now create physics bodies with properly computed transforms
   for (const Entity& e : clone->m_EntityList) {
      EntityID id = e.GetID();
      auto& data = clone->m_Entities[id];

      // Only create physics bodies for entities with colliders
      if (data.Collider) {
          // Update collider size based on entity scale for box shapes
          if (data.Collider->ShapeType == ColliderShape::Box) {
              data.Collider->Size = glm::abs(data.Collider->Size * data.Transform.Scale);
          }
          data.Collider->BuildShape(data.Mesh && data.Mesh->mesh ? data.Mesh->mesh.get() : nullptr);
          clone->CreatePhysicsBody(id, data.Transform, *data.Collider);
      }
      }

   // Initialize scripts after cloning
   for (auto& [scriptPtr, entity] : toInitialize) {
      scriptPtr->Instance->OnCreate(entity);
      }

   // Debug: Print parent-child relationships
   std::cout << "[Scene] Cloned scene parent-child relationships:" << std::endl;
   for (const auto& [id, data] : clone->m_Entities) {
      if (data.Parent != INVALID_ENTITY_ID) {
         std::cout << "  Entity " << id << " -> Parent " << data.Parent << std::endl;
      }
   }

   std::cout << "[Scene] Cloned scene with " << clone->m_Entities.size() << " entities\n";
   return clone;
   }



void Scene::OnStop() {
    // Destroy bodies stored in component data (new system)
    for (auto& [id, data] : m_Entities) {
        // Dynamic / kinematic bodies
        if (data.RigidBody && !data.RigidBody->BodyID.IsInvalid()) {
            Physics::Get().DestroyBody(data.RigidBody->BodyID);
            data.RigidBody->BodyID = JPH::BodyID();
        }
        // Static bodies
        if (data.StaticBody && !data.StaticBody->BodyID.IsInvalid()) {
            Physics::Get().DestroyBody(data.StaticBody->BodyID);
            data.StaticBody->BodyID = JPH::BodyID();
        }
    }

    // Destroy any bodies that are still tracked in the legacy map
    for (auto [id, bodyID] : m_BodyMap)
        Physics::Get().DestroyBody(bodyID);
    m_BodyMap.clear();
}


void Scene::DestroyPhysicsBody(EntityID id) {
    auto* data = GetEntityData(id);
    if (!data) return;

    JPH::BodyID bodyID;

    // Check RigidBody component first
    if (data->RigidBody && !data->RigidBody->BodyID.IsInvalid()) {
        bodyID = data->RigidBody->BodyID;
        data->RigidBody->BodyID = JPH::BodyID();
    }
    // Check StaticBody component
    else if (data->StaticBody && !data->StaticBody->BodyID.IsInvalid()) {
        bodyID = data->StaticBody->BodyID;
        data->StaticBody->BodyID = JPH::BodyID();
    }
    // Fallback to old m_BodyMap
    else {
        auto it = m_BodyMap.find(id);
        if (it != m_BodyMap.end()) {
            bodyID = it->second;
            m_BodyMap.erase(it);
        } else {
            return; // No body found
        }
    }

    if (!bodyID.IsInvalid()) {
        Physics::Get().DestroyBody(bodyID);
        std::cout << "[Scene] Destroyed physics body for Entity " << id << std::endl;
    }
}



void Scene::CreatePhysicsBody(EntityID id, const TransformComponent& transform, const ColliderComponent& collider) {
   if (!collider.Shape) {
      std::cerr << "[Scene] Cannot create physics body: shape is null\n";
      return;
      }

   auto* data = GetEntityData(id);
   if (!data) return;

   // Check if a physics body already exists for this entity
   if ((data->RigidBody && !data->RigidBody->BodyID.IsInvalid()) ||
      (data->StaticBody && !data->StaticBody->BodyID.IsInvalid()) ||
      m_BodyMap.find(id) != m_BodyMap.end()) {
      std::cout << "[Scene] Physics body already exists for Entity " << id << ", skipping creation\n";
      return;
      }

   // Combine world transform with collider offset
   glm::mat4 world = transform.WorldMatrix * glm::translate(glm::mat4(1.0f), collider.Offset);

   // --- Decompose matrix into position and rotation ---
   glm::vec3 pos, scale, skew;
   glm::quat rot;
   glm::vec4 perspective;
   if (!glm::decompose(world, scale, rot, pos, skew, perspective)) {
      std::cerr << "[Scene] Failed to decompose transform for Entity " << id << "\n";
      return;
      }

   // Convert to Jolt types
   JPH::RVec3 joltPosition(pos.x, pos.y, pos.z);
   JPH::Quat joltRotation(rot.x, rot.y, rot.z, rot.w);

   // Determine motion type
   JPH::EMotionType motionType = JPH::EMotionType::Static;
   if (data->RigidBody) {
      motionType = data->RigidBody->IsKinematic
         ? JPH::EMotionType::Kinematic
         : JPH::EMotionType::Dynamic;
      }

   // Print debug info
   std::cout << "[Scene] Creating " << (motionType == JPH::EMotionType::Static ? "Static" :
      motionType == JPH::EMotionType::Kinematic ? "Kinematic" : "Dynamic")
      << " body for Entity " << id
      << " at position (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";

   // Create body (specify object layer 0 for static, 1 for moving)
   uint8_t objectLayer = (motionType == JPH::EMotionType::Static) ? 0 : 1;
   JPH::BodyCreationSettings settings(collider.Shape, joltPosition, joltRotation, motionType, objectLayer);
   // Set friction: prefer RigidBody value, fall back to StaticBody, otherwise default
   if (data->RigidBody)
      settings.mFriction = data->RigidBody->Friction;
   else if (data->StaticBody)
      settings.mFriction = data->StaticBody->Friction;
   else
      settings.mFriction = 0.5f;
   settings.mRestitution = data->RigidBody ? data->RigidBody->Restitution : data->StaticBody ? data->StaticBody->Restitution : 0.0f;
   settings.mAllowSleeping = true;
   settings.mIsSensor = collider.IsTrigger;

   if (data->RigidBody) {
      settings.mMotionQuality = JPH::EMotionQuality::LinearCast; // Optional: for fast-moving objects
      settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;
      settings.mMassPropertiesOverride.mMass = data->RigidBody->Mass;
      }

   JPH::BodyInterface& bodyInterface = Physics::Get().GetBodyInterface();
   JPH::Body* body = bodyInterface.CreateBody(settings);

   if (!body) {
      std::cerr << "[Scene] Failed to create Jolt body for Entity " << id << std::endl;
      return;
      }

   JPH::BodyID bodyID = body->GetID();
   bodyInterface.AddBody(bodyID, JPH::EActivation::Activate);

   // Store the BodyID
   if (data->RigidBody) {
      data->RigidBody->BodyID = bodyID;
      }
   else if (data->StaticBody) {
      data->StaticBody->BodyID = bodyID;
      }
   else {
      m_BodyMap[id] = bodyID; // Fallback
      }

   std::cout << "[Scene] Created physics body with ID " << bodyID.GetIndex() << "\n";
   }

void Scene::Update(float dt) {
   UpdateTransforms();

   if (m_IsPlaying) {
      // Step physics simulation
      static int physicsStepCount = 0;
      physicsStepCount++;
      
      // Debug: Print gravity and step info (only for first few steps)
      if (physicsStepCount <= 5) {
         glm::vec3 gravity = Physics::Get().GetGravity();
         std::cout << "[Physics] Step " << physicsStepCount << " - dt: " << dt 
                   << " - Gravity: (" << gravity.x << ", " << gravity.y << ", " << gravity.z << ")" << std::endl;
      }
      
      Physics::Get().Step(dt);

      for (auto& [id, data] : m_Entities) {
         // Sync camera with transform
         if (data.Camera) {
            data.Camera->SyncWithTransform(data.Transform);
         }

         // Sync physics bodies with transforms
         if (data.RigidBody && !data.RigidBody->BodyID.IsInvalid()) {
            // For kinematic bodies, apply velocity
            if (data.RigidBody->IsKinematic) {
               // Apply linear and angular velocity
               Physics::Get().SetBodyLinearVelocity(data.RigidBody->BodyID, data.RigidBody->LinearVelocity);
               Physics::Get().SetBodyAngularVelocity(data.RigidBody->BodyID, data.RigidBody->AngularVelocity);
            } else {
               // For dynamic bodies, sync transform from physics
               glm::mat4 physicsTransform = Physics::Get().GetBodyTransform(data.RigidBody->BodyID);
               if (physicsTransform != glm::mat4(0.0f)) { // Check if valid transform
                  // Extract position and rotation from physics transform
                  glm::vec3 position = glm::vec3(physicsTransform[3]);
                  glm::mat3 rotationMatrix = glm::mat3(physicsTransform);
                  glm::vec3 rotation = glm::degrees(glm::eulerAngles(glm::quat_cast(rotationMatrix)));
                  
                  // Debug: Print physics transform sync (only for first few frames)
                  static int frameCount = 0;
                  if (frameCount < 10) {
                     std::cout << "[Physics] Frame " << frameCount << " - Entity " << id 
                               << " physics pos: (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
                  }
                  frameCount++;
                  
                  // Update entity transform
                  data.Transform.Position = position;
                  data.Transform.Rotation = rotation;
                  data.Transform.TransformDirty = true;
               }
            }
         }

         // Sync static bodies (they don't move, but we need to ensure they're positioned correctly)
         if (data.StaticBody && !data.StaticBody->BodyID.IsInvalid()) {
            // Static bodies don't move, but we can sync their initial position
            // This is mainly for when static bodies are created
         }

         for (auto& script : data.Scripts) {
            if (script.Instance)
               script.Instance->OnUpdate(dt);
            }
         }
      }
   }

bool Scene::HasComponent(const char* componentName) {
   for (const auto& entity : m_EntityList) {
      const EntityData* data = GetEntityData(entity.GetID());
      if (!data) continue;
      if (strcmp(componentName, "MeshComponent") == 0 && data->Mesh)
         return true;
      if (strcmp(componentName, "LightComponent") == 0 && data->Light)
         return true;
      if (strcmp(componentName, "ColliderComponent") == 0 && data->Collider)
         return true;
      if (strcmp(componentName, "CameraComponent") == 0 && data->Camera)
         return true;
      if (strcmp(componentName, "RigidBodyComponent") == 0 && data->RigidBody)
         return true;
      if (strcmp(componentName, "StaticBodyComponent") == 0 && data->StaticBody)
         return true;
      if (strcmp(componentName, "BlendShapeComponent") == 0 && data->BlendShapes)
         return true;
      if (strcmp(componentName, "SkeletonComponent") == 0 && data->Skeleton)
         return true;
      if (strcmp(componentName, "SkinningComponent") == 0 && data->Skinning)
         return true;
      }
   return false;
   }

Camera* Scene::GetActiveCamera() {
   int minPriority = std::numeric_limits<int>::max();
   EntityID selectedEntity = INVALID_ENTITY_ID;

   for (auto entity : m_EntityList) {
      auto* data = GetEntityData(entity.GetID());
      if (data && data->Camera && data->Camera->Active) {
         if (data->Camera->priority < minPriority) {
            minPriority = data->Camera->priority;
            selectedEntity = entity.GetID();
            }
         }
      }

   if (selectedEntity != INVALID_ENTITY_ID) {
      auto* entityData = GetEntityData(selectedEntity);
      return entityData ? &entityData->Camera->Camera : nullptr;
      }

   return nullptr;
   }

   
