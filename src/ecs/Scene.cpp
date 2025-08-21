#include "Scene.h"
#include "scripting/DotNetHost.h"
#include "EntityData.h"
#include <algorithm>
#include <functional>
#include <filesystem>
namespace fs = std::filesystem;
#include <rendering/ModelLoader.h>
#include <rendering/StandardMeshManager.h>
#include "ecs/AnimationComponents.h"
#include "ecs/ParticleEmitterSystem.h"
#include "ecs/SkinningSystem.h"
#include <rendering/TextureLoader.h>
#include <rendering/MaterialManager.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include "scripting/ManagedScriptComponent.h"
#include "scripting/ScriptReflection.h"
#include "scripting/ScriptReflectionInterop.h"
#include "animation/AvatarDefinition.h"
#include "animation/AnimationSystem.h"
#include "animation/AnimationAsset.h"
#include "animation/SkeletonBinding.h"
#include "animation/AnimationSerializer.h"
#include "animation/AnimationPlayerComponent.h"
#include <nlohmann/json.hpp>
#include <fstream>


#include <sstream> // Include for std::ostringstream
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <windows.h>
#include "animation/AvatarSerializer.h"
#include "serialization/Serializer.h"

#include "jobs/JobSystem.h"
#include "jobs/ParallelFor.h"
#include <jobs/Jobs.h>
#include "pipeline/AssetLibrary.h"    
#include "utils/Profiler.h"
#include <prefab/PrefabAPI.h>
#include "animation/ik/IKSystem.h"
// --- Kernels --------------------------------------------------------------------------------


struct RootArgs {
   Scene* scene;
   const std::vector<EntityID>* level;   // level 0 (roots)
   std::vector<uint8_t>* recomputed;     // size >= scene->m_NextID
   };

static inline void RootsKernel(const RootArgs& a, size_t start, size_t count) {
   for (size_t i = start; i < start + count; ++i) {
      EntityID id = (*a.level)[i];
      auto* d = a.scene->GetEntityData(id);
      if (!d) continue;
      const bool wasDirty = d->Transform.TransformDirty;
      if (wasDirty) d->Transform.CalculateLocalMatrix();
      d->Transform.WorldMatrix = d->Transform.LocalMatrix; // root: parent = I
      d->Transform.TransformDirty = false;
      (*a.recomputed)[id] = wasDirty ? 1u : 0u;
      }
   }

struct PropArgs {
   Scene* scene;
   const std::vector<EntityID>* level;       // current level
   std::vector<uint8_t>* recomputed;         // per-entity "updated this frame" flag
   };

static inline void PropagateKernel(const PropArgs& a, size_t start, size_t count) {
   for (size_t i = start; i < start + count; ++i) {
      EntityID id = (*a.level)[i];
      auto* d = a.scene->GetEntityData(id);
      if (!d) continue;

      // parent info (guaranteed processed on a previous level)
      glm::mat4 parentWorld = glm::mat4(1.0f);
      bool parentUpdated = false;
      if (d->Parent != -1) {
         auto* p = a.scene->GetEntityData(d->Parent);
         parentWorld = p ? p->Transform.WorldMatrix : glm::mat4(1.0f);
         parentUpdated = p ? ((*a.recomputed)[d->Parent] != 0) : false;
         }

      const bool wasDirty = d->Transform.TransformDirty;
      const bool needs = wasDirty || parentUpdated;
      if (needs) {
         if (wasDirty) d->Transform.CalculateLocalMatrix(); // local changed
         d->Transform.WorldMatrix = parentWorld * d->Transform.LocalMatrix;
         d->Transform.TransformDirty = false;
         }
      (*a.recomputed)[id] = needs ? 1u : 0u;
      }
   }

// --- Build breadth levels from your existing parent/children lists
static inline void BuildHierarchyLevels(Scene& scene,
   std::vector<std::vector<EntityID>>& levels)
   {
   levels.clear();
   std::vector<EntityID> roots;
   for (const auto& e : scene.GetEntities()) {
      auto* d = scene.GetEntityData(e.GetID());
      if (d && d->Parent == -1) roots.push_back(e.GetID());
      }
   if (roots.empty()) return;
   levels.push_back(std::move(roots));

   // BFS: each level is the concatenation of all children from the previous level
   for (;;) {
      std::vector<EntityID> next;
      for (EntityID id : levels.back()) {
         auto* d = scene.GetEntityData(id);
         if (!d) continue;
         next.insert(next.end(), d->Children.begin(), d->Children.end());
         }
      if (next.empty()) break;
      levels.push_back(std::move(next));
      }
   }
// -----------------------------------------------------------------------------------------

Scene* Scene::CurrentScene = nullptr;

Entity Scene::CreateEntity(const std::string& name) {
   EntityID id = m_NextID++;
   EntityData data;

   // Use the provided name unless a collision exists; then append _<id>
   bool nameExists = std::any_of(m_Entities.begin(), m_Entities.end(), [&](const auto& pair) {
       return pair.second.Name == name;
   });
   if (nameExists) {
       std::ostringstream oss;
       oss << name << "_" << id;
       data.Name = oss.str();
   } else {
       data.Name = name;
   }

   m_Entities.emplace(id, std::move(data));

   Entity entity(id, this);
   m_EntityList.push_back(entity);

   return entity;
}

// Create an entity preserving the exact provided name (no suffixing). For deserialization.
Entity Scene::CreateEntityExact(const std::string& name) {
   EntityID id = m_NextID++;
   EntityData data;
   data.Name = name;

   m_Entities.emplace(id, std::move(data));

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

    // 4. Clean up allocated components (unique_ptr handles deletion)
    if (data->Mesh) {
        data->Mesh->mesh = nullptr;
        data->Mesh->material.reset();
        data->Mesh->BlendShapes = nullptr;
        data->Mesh.reset();
    }
    if (data->Light) {
        data->Light.reset();
    }
    if (data->Collider) {
        data->Collider.reset();
    }
    if (data->Camera) {
        data->Camera.reset();
    }
    if (data->RigidBody) {
        data->RigidBody.reset();
    }
    if (data->StaticBody) {
        data->StaticBody.reset();
    }
    if (data->Emitter) {
        if (ps::isValid(data->Emitter->Handle)) {
            ps::destroyEmitter(data->Emitter->Handle);
            data->Emitter->Handle = { uint16_t{UINT16_MAX} };
        }
        data->Emitter->Uniforms.reset();
        data->Emitter->Enabled = false;
        data->Emitter.reset();
    }
    if (data->BlendShapes) {
        data->BlendShapes.reset();
    }
    if (data->Skeleton) {
        data->Skeleton.reset();
    }
    if (data->Skinning) {
        data->Skinning.reset();
    }

    // 5. Clean up scripts
    for (auto& script : data->Scripts) {
        // Note: Script_Destroy is not exposed to C++, so we just clear the vector
        // The shared_ptr will handle cleanup of native script instances
        // For managed scripts, the GCHandle cleanup happens in C# when the script is destroyed
    }
    data->Scripts.clear();

    // 6. Remove from entity collections (erase from list first to avoid iterator use during render)
    m_EntityList.erase(
        std::remove_if(m_EntityList.begin(), m_EntityList.end(),
            [&](const Entity& e) { return e.GetID() == id; }),
        m_EntityList.end());
    m_Entities.erase(id);

    std::cout << "[Scene] Removed entity " << id << " and all its children" << std::endl;
}

EntityData* Scene::GetEntityData(EntityID id) {
    auto it = m_Entities.find(id);
    return (it != m_Entities.end()) ? &it->second : nullptr;
}
 
void Scene::QueueRemoveEntity(EntityID id) {
    // Allow duplicates; we'll dedupe when processing
    m_PendingRemovals.push_back(id);
}

void Scene::ProcessPendingRemovals() {
    if (m_PendingRemovals.empty()) return;
    // Deduplicate while preserving order of first occurrence
    std::unordered_set<EntityID> seen;
    std::vector<EntityID> unique;
    unique.reserve(m_PendingRemovals.size());
    for (EntityID id : m_PendingRemovals) {
        if (seen.insert(id).second) unique.push_back(id);
    }
    for (EntityID id : unique) {
        RemoveEntity(id);
    }
    m_PendingRemovals.clear();
}

Entity Scene::FindEntityByID(EntityID id) {
    auto it = std::find_if(m_EntityList.begin(), m_EntityList.end(),
        [&](const Entity& e) { return e.GetID() == id; });
    return (it != m_EntityList.end()) ? *it : Entity();
}

Entity Scene::CreateLight(const std::string& name, LightType type, const glm::vec3& color, float intensity) {
   Entity entity = CreateEntity(name);
   if (auto* data = GetEntityData(entity.GetID())) {
      data->Light = std::make_unique<LightComponent>(type, color, intensity);
      }
   return entity;
   }


EntityID Scene::InstantiateAsset(const std::string& path, const glm::vec3& position) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf") {
       return InstantiateModel(path, position);
       }
    else if (ext == ".prefab") {
        // Support both legacy single-entity and subtree prefab formats
        EntityID rootId = Serializer::LoadPrefabToScene(path, *this);
        if (rootId == -1 || rootId == 0) {
            std::cerr << "[Scene] Failed to load prefab: " << path << std::endl;
            return -1;
        }
        if (auto* d = GetEntityData(rootId)) {
            d->Transform.Position = position;
            MarkTransformDirty(rootId);
        }
        return rootId;
    }
    else if (ext == ".json") {
        // New authoring prefab format
        EntityID rootId = InstantiatePrefabFromAuthoringPath(path, *this);
        if (rootId == -1 || rootId == 0) {
            std::cerr << "[Scene] Failed to instantiate authoring prefab: " << path << std::endl;
            return -1;
        }
        if (auto* d = GetEntityData(rootId)) {
            d->Transform.Position = position;
            MarkTransformDirty(rootId);
        }
        return rootId;
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

        data->Mesh = std::make_unique<MeshComponent>();
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
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    const aiScene* aScene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_LimitBoneWeights |
        aiProcess_GlobalScale);

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
    // Decompose FBX root node transform to preserve authored placement
    glm::vec3 rootT, rootS, rootSkew; glm::vec4 rootPersp; glm::quat rootR;
    glm::mat4 rootLocal   = glm::mat4(1.0f); // will be set below prior to traversal
    glm::mat4 invRoot     = glm::mat4(1.0f);
    {
        // Helper: Assimp (row-major) -> GLM (column-major) exists below. We'll use it here after it's defined.
    }

    //--------------------------------------------------------------------
    // Build map of meshIndex -> transform relative to the FBX root
    //--------------------------------------------------------------------
    std::vector<glm::mat4> meshTransforms(aScene->mNumMeshes, glm::mat4(1.0f));
    std::vector<std::string> meshEntityNames(aScene->mNumMeshes, std::string());

    // Helper: Assimp (row-major) -> GLM (column-major)
    auto AiToGlm = [](const aiMatrix4x4& m) {
        // Keep consistent with ModelLoader's AiToGlm: construct directly (no transpose)
        return glm::mat4(
            m.a1, m.b1, m.c1, m.d1,
            m.a2, m.b2, m.c2, m.d2,
            m.a3, m.b3, m.c3, m.d3,
            m.a4, m.b4, m.c4, m.d4);
    };

    rootLocal   = AiToGlm(aScene->mRootNode->mTransformation);
    // Normalize away authoring unit scaling on the root if present (FBX UnitScaleFactor)
    float unitScale = 1.0f;
    if (aScene->mMetaData) {
        double s = 1.0;
        if (aScene->mMetaData->Get("UnitScaleFactor", s)) unitScale = static_cast<float>(s);
    }
    invRoot     = glm::inverse(rootLocal);

    // Set root entity transform from FBX root transform, add requested spawn offset
    glm::decompose(rootLocal, rootS, rootR, rootT, rootSkew, rootPersp);
    rootData->Transform.Position = rootT + rootPosition;
    rootData->Transform.RotationQ = glm::normalize(rootR);
    rootData->Transform.UseQuatRotation = true;
    // Keep Euler for inspector display
    rootData->Transform.Rotation = glm::degrees(glm::eulerAngles(rootR));
    // Clamp unreasonable global scaling from FBX (e.g., 100)
    if (rootS.x > 50.0f || rootS.y > 50.0f || rootS.z > 50.0f)
        rootS = glm::vec3(1.0f);
    rootData->Transform.Scale    = rootS;
    rootData->Transform.TransformDirty = true;

    // Recursive lambda to accumulate transforms
    std::function<void(aiNode*, const glm::mat4&)> traverse;
    traverse = [&](aiNode* node, const glm::mat4& parentTransform) {
        glm::mat4 local  = AiToGlm(node->mTransformation);
        glm::mat4 global = parentTransform * local;
        // Keep meshes in the model's local space; entity root carries the FBX root transform.
        glm::mat4 relative = invRoot * global;

        // Store relative transform and remember a name for entities from this node
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            unsigned int meshIndex = node->mMeshes[i];
            if (meshIndex < meshTransforms.size())
                meshTransforms[meshIndex] = relative;
            if (meshIndex < meshEntityNames.size()) {
                // Prefer node name for hierarchy clarity; fallback applied later
                meshEntityNames[meshIndex] = node->mName.C_Str();
            }
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
        // Build name->index map for bones we know about
        std::unordered_map<std::string, int> boneNameToIndex;
        boneNameToIndex.reserve(model.BoneNames.size());
        for (int i = 0; i < (int)model.BoneNames.size(); ++i)
            boneNameToIndex[model.BoneNames[i]] = i;

        // Build name->aiNode* map for the whole scene to query parents
        std::unordered_map<std::string, aiNode*> nodeByName;
        std::function<void(aiNode*)> gatherNodes = [&](aiNode* n){
            nodeByName[n->mName.C_Str()] = n;
            for (unsigned int c = 0; c < n->mNumChildren; ++c)
                gatherNodes(n->mChildren[c]);
        };
        gatherNodes(aScene->mRootNode);

        // Compute parent indices from the Assimp node hierarchy
        std::vector<int> parentIndex(model.BoneNames.size(), -1);
        for (size_t i = 0; i < model.BoneNames.size(); ++i) {
            auto itNode = nodeByName.find(model.BoneNames[i]);
            if (itNode != nodeByName.end()) {
                aiNode* p = itNode->second->mParent;
                while (p) {
                    auto itBI = boneNameToIndex.find(p->mName.C_Str());
                    if (itBI != boneNameToIndex.end()) { parentIndex[i] = itBI->second; break; }
                    p = p->mParent;
                }
            }
        }

        // No runtime toggle: default to inverse-bind initialization

        // Create skeleton root and bone entities
        Entity skeletonRootEnt = CreateEntity("SkeletonRoot");
        skeletonRootID = skeletonRootEnt.GetID();
        SetParent(skeletonRootID, rootID);

        auto* skelData = GetEntityData(skeletonRootID);
        skelData->Skeleton = std::make_unique<SkeletonComponent>();
        skelData->Skeleton->InverseBindPoses = model.InverseBindPoses;

        // Fill exact bind-pose globals (no decomposition/round-trips)
        skelData->Skeleton->BindPoseGlobals.resize(model.InverseBindPoses.size());
        for (size_t i = 0; i < model.InverseBindPoses.size(); ++i) {
            skelData->Skeleton->BindPoseGlobals[i] = glm::inverse(model.InverseBindPoses[i]);
        }


        skelData->Skeleton->BoneParents = parentIndex;
        for (int i = 0; i < (int)model.BoneNames.size(); ++i)
            skelData->Skeleton->BoneNameToIndex[model.BoneNames[i]] = i;
        skelData->Skeleton->BoneNames = model.BoneNames;
        // Populate stable skeleton GUID and per-joint GUIDs
        {
            ClaymoreGUID skelGuid = AssetLibrary::Instance().GetGUIDForPath(path);
            skelData->Skeleton->SkeletonGuid = skelGuid;
            ComputeSkeletonJointGuids(*skelData->Skeleton);
        }

        // Pre-create all bone entities
        std::vector<EntityID> boneEntities(model.BoneNames.size(), INVALID_ENTITY_ID);
        for (size_t b = 0; b < model.BoneNames.size(); ++b) {
            Entity boneEnt = CreateEntity(model.BoneNames[b]);
            boneEntities[b] = boneEnt.GetID();
        }

        // Parent bones and set local transforms from inverse bind matrices
        for (size_t b = 0; b < model.BoneNames.size(); ++b) {
            EntityID boneID = boneEntities[b];
            int pIdx = parentIndex[b];
            EntityID parentEntity = (pIdx >= 0) ? boneEntities[pIdx] : skeletonRootID;
            SetParent(boneID, parentEntity);
            // Classic: derive local from inverse bind matrices (global = inverse(invBind))
            glm::mat4 thisGlobal = glm::inverse(model.InverseBindPoses[b]);
            glm::mat4 parentGlobal = (pIdx >= 0) ? glm::inverse(model.InverseBindPoses[pIdx]) : glm::mat4(1.0f);
            glm::mat4 localBind = glm::inverse(parentGlobal) * thisGlobal;

            glm::vec3 t, scale, skew; glm::vec4 persp; glm::quat rq;
            glm::decompose(localBind, scale, rq, t, skew, persp);
            auto* boneData = GetEntityData(boneID);
            if (boneData) {
                boneData->Transform.Position = t;
                boneData->Transform.Scale    = scale;
                boneData->Transform.RotationQ = glm::normalize(rq);
                boneData->Transform.UseQuatRotation = true;
                // Optional: populate Euler for read-only inspector
                boneData->Transform.Rotation = glm::degrees(glm::eulerAngles(rq));
                boneData->Transform.TransformDirty = true;
            }
        }

        skelData->Skeleton->BoneEntities = boneEntities;

        // Try to load a prebuilt .avatar next to the model if present; otherwise, build via heuristics
        skelData->Skeleton->Avatar = std::make_unique<cm::animation::AvatarDefinition>();
        {
            std::filesystem::path p(path);
            std::string avatarPath = (p.parent_path() / (p.stem().string() + ".avatar")).string();
            if (!cm::animation::LoadAvatar(*skelData->Skeleton->Avatar, avatarPath)) {
                cm::animation::avatar_builders::BuildFromSkeleton(*skelData->Skeleton, *skelData->Skeleton->Avatar, true);
            }
        }

        // Auto-add an AnimationPlayerComponent at the skeleton root if the source FBX has animations
        // Load the first unified .anim next to the FBX into the player's first state so it can play without a controller
        Assimp::Importer animImporter;
        const aiScene* fbxForAnims = animImporter.ReadFile(path,
            aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_LimitBoneWeights | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_FlipUVs);
        if (fbxForAnims && fbxForAnims->mNumAnimations > 0) {
            if (!skelData->AnimationPlayer) skelData->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>();
            // Look for a unified .anim next to the FBX using the pattern <fbxname>_*.anim; fallback to first .anim
            std::filesystem::path p(path);
            std::filesystem::path dir = p.parent_path();
            std::string stem = p.stem().string();
            std::string chosenAnim;
            for (auto& entry : std::filesystem::directory_iterator(dir)) {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".anim") {
                    if (entry.path().filename().string().rfind(stem + "_", 0) == 0) { // starts with stem + "_"
                        chosenAnim = entry.path().string();
                        break;
                    }
                    if (chosenAnim.empty()) chosenAnim = entry.path().string();
                }
            }
            // Initialize the first active state with loop on by default
            if (skelData->AnimationPlayer->ActiveStates.empty()) skelData->AnimationPlayer->ActiveStates.push_back({});
            skelData->AnimationPlayer->ActiveStates.front().Loop = true;

            if (!chosenAnim.empty()) {
                // Load unified asset; fallback to legacy skeletal clip if needed
                cm::animation::AnimationAsset asset = cm::animation::LoadAnimationAsset(chosenAnim);
                if (asset.tracks.empty()) {
                    cm::animation::AnimationClip legacy = cm::animation::LoadAnimationClip(chosenAnim);
                    if (!legacy.BoneTracks.empty()) {
                        asset = cm::animation::WrapLegacyClipAsAsset(legacy);
                    }
                }
                auto assetPtr = std::make_shared<cm::animation::AnimationAsset>(std::move(asset));
                // Cache and bind into the first active state so non-controller playback works
                skelData->AnimationPlayer->CachedAssets[0] = assetPtr;
                skelData->AnimationPlayer->ActiveStates.front().Asset = assetPtr.get();
                skelData->AnimationPlayer->ActiveStates.front().Time = 0.0f;
                skelData->AnimationPlayer->ActiveStates.front().Weight = 1.0f;
                skelData->AnimationPlayer->Controller.reset();
                skelData->AnimationPlayer->CurrentStateId = -1;
                skelData->AnimationPlayer->AnimatorMode = cm::animation::AnimationPlayerComponent::Mode::AnimationPlayerAnimated;
                skelData->AnimationPlayer->SingleClipPath = chosenAnim;
            }
        }
    }



    //--------------------------------------------------------------------
    // Create one entity per mesh as child of the root entity
    //--------------------------------------------------------------------
    // If this FBX has no skeleton, apply an axis correction so the model isn't upside down.
    // Many DCC tools export FBX with an orientation that ends up inverted in our +Y up world.
    // A 180-degree rotation around X fixes this while preserving winding order.
    glm::mat4 nonSkinnedAxisFix = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    bool applyAxisFix = model.BoneNames.empty();
    if (applyAxisFix) {
        for (auto& mt : meshTransforms) {
            mt = nonSkinnedAxisFix * mt;
        }
    }

    for (size_t i = 0; i < model.Meshes.size(); ++i) {
        const auto& meshPtr = model.Meshes[i];
        if (!meshPtr) continue;

        // Pick an entity name derived from FBX content
        std::string desiredName = (i < meshEntityNames.size()) ? meshEntityNames[i] : std::string();
        if (desiredName.empty()) {
            // Fallback to Assimp mesh name if node name was empty
            if (i < aScene->mNumMeshes) {
                desiredName = aScene->mMeshes[i]->mName.C_Str();
            }
        }
        if (desiredName.empty()) desiredName = std::string("Mesh_") + std::to_string(i);

        Entity meshEntity = CreateEntity(desiredName);
        EntityID meshID = meshEntity.GetID();

        // >>> CHANGE: parent skinned meshes to SkeletonRoot (not ImportedModel root)
        const bool isSkinned = meshPtr->HasSkinning();
        SetParent(meshID, (isSkinned && skeletonRootID != -1) ? skeletonRootID : rootID);

        auto* meshData = GetEntityData(meshID);
        if (!meshData) continue;

        // Decompose the mesh-local transform you already computed (relative to FBX root)
        glm::vec3 translation, scale, skew;
        glm::vec4 perspective;
        glm::quat rotationQuat;
        glm::decompose(meshTransforms[i], scale, rotationQuat, translation, skew, perspective);

        meshData->Transform.Position = translation;
        meshData->Transform.Scale = scale;
        // For non-bone entities keep Euler as primary unless needed
            meshData->Transform.Rotation = glm::degrees(glm::eulerAngles(rotationQuat));
        meshData->Transform.TransformDirty = true;

        auto mat = (i < model.Materials.size() && model.Materials[i]) ? model.Materials[i]
            : MaterialManager::Instance().CreateDefaultPBRMaterial();
        meshData->Mesh = std::make_unique<MeshComponent>(meshPtr, desiredName, mat);

        // Fill an AssetReference for this submesh so it can serialize via GUID, not name
        // We treat each submesh as (guid of model, fileID = submesh index, type = Mesh)
        ClaymoreGUID guid = AssetLibrary::Instance().GetGUIDForPath(path);
        if (guid.high != 0 || guid.low != 0) {
            meshData->Mesh->meshReference = AssetReference(guid, static_cast<int32_t>(i), static_cast<int32_t>(AssetType::Mesh));
        }

            if (isSkinned) {
            meshData->Skinning = std::make_unique<SkinningComponent>();
                meshData->Skinning->SkeletonRoot = skeletonRootID;
                meshData->Skinning->Palette.resize(model.BoneNames.size(), glm::mat4(1.0f));
            }

            if (i < model.BlendShapes.size() && !model.BlendShapes[i].Shapes.empty()) {
                auto bsPtr = std::make_unique<BlendShapeComponent>(model.BlendShapes[i]);
                meshData->Mesh->BlendShapes = bsPtr.get();
                meshData->BlendShapes = std::move(bsPtr);
            }
    }

    std::cout << "[Scene] Imported model with " << model.Meshes.size() << " mesh entities under root " << rootID << std::endl;
    return rootID;
}

// Fast path for models imported via cached binaries (.meta/.meshbin/.skelbin)
EntityID Scene::InstantiateModelFast(const std::string& metaPath, const glm::vec3& position) {
    try {
        nlohmann::json j;
        std::ifstream in(metaPath);
        if (!in.is_open()) {
            std::cerr << "[Scene] InstantiateModelFast: failed to open meta '" << metaPath << "'" << std::endl;
            return -1;
        }
        in >> j;
        in.close();

        std::string source = j.value("source", std::string());
        if (source.empty()) {
            namespace fs = std::filesystem;
            fs::path mp(metaPath);
            fs::path guess = mp.parent_path() / (mp.stem().string() + ".fbx");
            if (fs::exists(guess)) source = guess.string();
        }
        if (source.empty()) return -1;
        return InstantiateModel(source, position);
    } catch (const std::exception& e) {
        std::cerr << "[Scene] InstantiateModelFast error: " << e.what() << std::endl;
        return -1;
    }
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
   // Mark child subtree dirty so transforms recompute relative to new parent
   MarkTransformDirty(child);
   }


void Scene::UpdateTransforms()
   {
   // Build levels (roots -> leaves). Later you can cache this and update when SetParent/RemoveEntity runs.
   std::vector<std::vector<EntityID>> levels;
   BuildHierarchyLevels(*this, levels);

   if (levels.empty()) return;

   // One byte per possible EntityID (IDs are monotonic up to m_NextID)
   std::vector<uint8_t> recomputed(m_NextID, 0);

   // Level 0: roots
   {
   RootArgs args{ this, &levels[0], &recomputed };
   parallel_for(Jobs(), size_t{0}, levels[0].size(), size_t{1024},
      [&](size_t s, size_t c) { RootsKernel(args, s, c); });
   }

   // Levels 1..N: propagate world = parentWorld * local
   for (size_t L = 1; L < levels.size(); ++L) {
      PropArgs args{ this, &levels[L], &recomputed };
      parallel_for(Jobs(), size_t{0}, levels[L].size(), size_t{2048},
         [&](size_t s, size_t c) { PropagateKernel(args, s, c); });
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
   // Copy environment so play mode preserves edit-time settings
   clone->m_Environment = this->m_Environment;

   // Clone entities
   for (const Entity& e : m_EntityList) {
      EntityID id = e.GetID();

      clone->m_EntityList.emplace_back(id, clone.get());
      clone->m_Entities.emplace(id, m_Entities.at(id).DeepCopy(id, clone.get()));
      
      auto& data = clone->m_Entities[id];

      // Mark transform as dirty so world matrices are computed
      data.Transform.TransformDirty = true;

      // Ensure animator runtime flags are initialized for play mode
      if (data.AnimationPlayer) {
          // Reset one-shot init gate so PlayOnStart will apply in runtime
          data.AnimationPlayer->_InitApplied = false;
          // Seed playing state from PlayOnStart for Animation Player mode
          if (data.AnimationPlayer->AnimatorMode == cm::animation::AnimationPlayerComponent::Mode::AnimationPlayerAnimated) {
              data.AnimationPlayer->IsPlaying = data.AnimationPlayer->PlayOnStart;
              if (!data.AnimationPlayer->ActiveStates.empty() && data.AnimationPlayer->PlayOnStart) {
                  data.AnimationPlayer->ActiveStates.front().Time = 0.0f;
              }
          }
      }

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

   // Apply reflected property values to managed scripts, then initialize
   for (auto& [scriptPtr, entity] : toInitialize) {
      if (scriptPtr && scriptPtr->Instance &&
          scriptPtr->Instance->GetBackend() == ScriptBackend::Managed) {
         auto managed = std::dynamic_pointer_cast<ManagedScriptComponent>(scriptPtr->Instance);
         if (managed && SetManagedFieldPtr) {
            void* handle = managed->GetHandle();
            auto& properties = ScriptReflection::GetScriptProperties(scriptPtr->ClassName);
            for (auto& property : properties) {
               switch (property.type) {
                  case PropertyType::Int: {
                     int v = std::get<int>(property.currentValue);
                     SetManagedFieldPtr(handle, property.name.c_str(), &v);
                     break;
                  }
                  case PropertyType::Float: {
                     float v = std::get<float>(property.currentValue);
                     SetManagedFieldPtr(handle, property.name.c_str(), &v);
                     break;
                  }
                  case PropertyType::Bool: {
                     bool v = std::get<bool>(property.currentValue);
                     SetManagedFieldPtr(handle, property.name.c_str(), &v);
                     break;
                  }
                  case PropertyType::String: {
                     const std::string& s = std::get<std::string>(property.currentValue);
                     const char* cstr = s.c_str();
                     SetManagedFieldPtr(handle, property.name.c_str(), (void*)cstr);
                     break;
                  }
                  case PropertyType::Vector3: {
                     glm::vec3 v = std::get<glm::vec3>(property.currentValue);
                     SetManagedFieldPtr(handle, property.name.c_str(), &v);
                     break;
                  }
                  case PropertyType::Entity: {
                     int id = std::get<int>(property.currentValue);
                     SetManagedFieldPtr(handle, property.name.c_str(), &id);
                     break;
                  }
               }
            }
         }
      }
      // Now call OnCreate so scripts see the configured values at startup
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
    for (const auto& kv : m_BodyMap)
        Physics::Get().DestroyBody(kv.second);
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
   ScopedTimer tScene("Scene/Update Total");
   static bool once = (std::cout << "[C++] Scene::Update thread: " << GetCurrentThreadId() << "\n", true);
   // Ensure any queued deletions are processed at a safe point each frame
   ProcessPendingRemovals();

   // In play mode, evaluate animations before recomputing world transforms
   if (m_IsPlaying) {
      ScopedTimer t("Animation");
      cm::animation::AnimationSystem::Update(*this, dt);
   }

   // IK pass: after animation sampling, before transforms/skinning
   if (m_IsPlaying) {
      cm::animation::ik::IKSystem::Get().SolveAndBlend(*this, dt);
   }

   // Recompute world transforms after potential animation updates
   {
      ScopedTimer t("Transforms");
      UpdateTransforms();
   }

   // Update GPU skinning palette after transforms
   if (m_IsPlaying) {
      // Step skinning after animation so GPU palettes reflect latest pose
      ScopedTimer t("Skinning");
      SkinningSystem::Update(*this);
   }

   // Update particle emitters so they preview both in edit and play mode
   {
      ScopedTimer t("Particles");
      ecs::ParticleEmitterSystem::Get().Update(*this, dt);
   }

   // Update navigation agents and debug drawing
   {
      ScopedTimer t("Navigation");
      nav::Navigation::Get().Update(*this, dt);
   }

   extern void(__stdcall * EnsureInstalledPtr)();    // forward if needed, or capture in a singleton
   extern void(__stdcall * FlushSyncContextPtr)();
   extern void(__stdcall * ClearSyncContextPtr)();

   if (EnsureInstalledPtr) EnsureInstalledPtr();

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
      
      {
         ScopedTimer t("Physics/Step");
         Physics::Get().Step(dt);
      }

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
            if (script.Instance) {
               auto scriptStart = std::chrono::high_resolution_clock::now();
               script.Instance->OnUpdate(dt);
               auto scriptEnd = std::chrono::high_resolution_clock::now();
               double ms = std::chrono::duration<double, std::milli>(scriptEnd - scriptStart).count();
               Profiler::Get().RecordScriptSample(script.ClassName, ms);
            }
         }


         }

      // Flush managed SynchronizationContext so that await continuations run on the main thread
      if(FlushSyncContextPtr)
      {
         auto fnBits = reinterpret_cast<uintptr_t>(FlushSyncContextPtr);
         if(fnBits > 0x10000)
            FlushSyncContextPtr();
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
      if (strcmp(componentName, "CanvasComponent") == 0 && data->Canvas)
         return true;
      if (strcmp(componentName, "PanelComponent") == 0 && data->Panel)
         return true;
      if (strcmp(componentName, "ButtonComponent") == 0 && data->Button)
         return true;
      }
   return false;
   }

Camera* Scene::GetActiveCamera() {
   int minPriority = std::numeric_limits<int>::max();
   EntityID selectedEntity = INVALID_ENTITY_ID;

   for (const auto& entity : m_EntityList) {
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

   
