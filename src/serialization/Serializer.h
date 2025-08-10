#pragma once
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "ecs/EntityData.h"
#include "ecs/UIComponents.h"
#include "rendering/MaterialManager.h"
#include "rendering/StandardMeshManager.h"

using json = nlohmann::json;
 
class Serializer {
public:
    // Scene serialization 
    static json SerializeScene(Scene& scene);
    static bool DeserializeScene(const json& data, Scene& scene);
    static bool SaveSceneToFile(Scene& scene, const std::string& filepath);
    static bool LoadSceneFromFile(const std::string& filepath, Scene& scene);

    // Prefab serialization
    static json SerializePrefab(const EntityData& entityData, Scene& scene);
    static bool DeserializePrefab(const json& data, EntityData& entityData, Scene& scene);
    static bool SavePrefabToFile(const EntityData& entityData, Scene& scene, const std::string& filepath);
    static bool LoadPrefabFromFile(const std::string& filepath, EntityData& entityData, Scene& scene);

    // Individual component serialization
    static json SerializeTransform(const TransformComponent& transform);
    static void DeserializeTransform(const json& data, TransformComponent& transform);
    
    static json SerializeMesh(const MeshComponent& mesh);
    static void DeserializeMesh(const json& data, MeshComponent& mesh);
    
    static json SerializeLight(const LightComponent& light);
    static void DeserializeLight(const json& data, LightComponent& light);
    
    static json SerializeCollider(const ColliderComponent& collider);
    static void DeserializeCollider(const json& data, ColliderComponent& collider);

    static json SerializeRigidBody(const RigidBodyComponent& rigidbody);
    static void DeserializeRigidBody(const json& data, RigidBodyComponent& rigidbody);

    static json SerializeStaticBody(const StaticBodyComponent& staticbody);
    static void DeserializeStaticBody(const json& data, StaticBodyComponent& staticbody);

     // Camera
     static json SerializeCamera(const CameraComponent& camera);
     static void DeserializeCamera(const json& data, CameraComponent& camera);

     // Terrain
     static json SerializeTerrain(const TerrainComponent& terrain);
     static void DeserializeTerrain(const json& data, TerrainComponent& terrain);

     // Particle Emitter
     static json SerializeParticleEmitter(const ParticleEmitterComponent& emitter);
     static void DeserializeParticleEmitter(const json& data, ParticleEmitterComponent& emitter);

     // UI components
     static json SerializeCanvas(const CanvasComponent& canvas);
     static void DeserializeCanvas(const json& data, CanvasComponent& canvas);
     static json SerializePanel(const PanelComponent& panel);
     static void DeserializePanel(const json& data, PanelComponent& panel);
     static json SerializeButton(const ButtonComponent& button);
     static void DeserializeButton(const json& data, ButtonComponent& button);

    // Script serialization
    static json SerializeScripts(const std::vector<ScriptInstance>& scripts);
    static void DeserializeScripts(const json& data, std::vector<ScriptInstance>& scripts);

    // Entity serialization
    static json SerializeEntity(EntityID id, Scene& scene);
    static EntityID DeserializeEntity(const json& data, Scene& scene);

private:
    // Helper functions
    static json SerializeVec3(const glm::vec3& vec);
    static glm::vec3 DeserializeVec3(const json& data);
    
    static json SerializeMat4(const glm::mat4& mat);
    static glm::mat4 DeserializeMat4(const json& data);
};