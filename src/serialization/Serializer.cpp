#include "Serializer.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "ecs/AnimationComponents.h"
#include "scripting/ScriptSystem.h"
#include "ecs/EntityData.h"
#include "pipeline/AssetLibrary.h"
#include "rendering/TextureLoader.h"
#include "ecs/UIComponents.h"

namespace fs = std::filesystem;

// Helper functions
json Serializer::SerializeVec3(const glm::vec3& vec) {
    return json{{"x", vec.x}, {"y", vec.y}, {"z", vec.z}};
}

glm::vec3 Serializer::DeserializeVec3(const json& data) {
    return glm::vec3{data["x"], data["y"], data["z"]};
}

json Serializer::SerializeMat4(const glm::mat4& mat) {
    json result = json::array();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.push_back(mat[i][j]);
        }
    }
    return result;
}

glm::mat4 Serializer::DeserializeMat4(const json& data) {
    glm::mat4 mat(1.0f);
    if (data.is_array() && data.size() == 16) {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                mat[i][j] = data[i * 4 + j];
            }
        }
    }
    return mat;
}

// Component serialization
json Serializer::SerializeTransform(const TransformComponent& transform) {
    json data;
    data["position"] = SerializeVec3(transform.Position);
    data["rotation"] = SerializeVec3(transform.Rotation);
    data["scale"] = SerializeVec3(transform.Scale);
    data["localMatrix"] = SerializeMat4(transform.LocalMatrix);
    data["worldMatrix"] = SerializeMat4(transform.WorldMatrix);
    data["transformDirty"] = transform.TransformDirty;
    return data;
}

void Serializer::DeserializeTransform(const json& data, TransformComponent& transform) {
    if (data.contains("position")) transform.Position = DeserializeVec3(data["position"]);
    if (data.contains("rotation")) transform.Rotation = DeserializeVec3(data["rotation"]);
    if (data.contains("scale")) transform.Scale = DeserializeVec3(data["scale"]);
    if (data.contains("localMatrix")) transform.LocalMatrix = DeserializeMat4(data["localMatrix"]);
    if (data.contains("worldMatrix")) transform.WorldMatrix = DeserializeMat4(data["worldMatrix"]);
    if (data.contains("transformDirty")) transform.TransformDirty = data["transformDirty"];
}

json Serializer::SerializeMesh(const MeshComponent& mesh) {
    json data;
    
    // Serialize both the old name-based system and new asset reference system
    data["meshName"] = mesh.MeshName;
    data["meshReference"] = mesh.meshReference;
    
    if (mesh.material) {
        data["materialName"] = mesh.material->GetName();
        // Store material properties if it's a PBR material
        if (auto pbr = std::dynamic_pointer_cast<PBRMaterial>(mesh.material)) {
            data["materialType"] = "PBR";
            // Note: Texture handles are runtime-specific, we'd need to store texture paths instead
            // This would require extending the Material system to track source paths
        }
    }
    return data;
}

void Serializer::DeserializeMesh(const json& data, MeshComponent& mesh) {
    // First try to load using the new asset reference system
    if (data.contains("meshReference")) {
        data["meshReference"].get_to(mesh.meshReference);
        
        // Load mesh from AssetLibrary using the reference
        mesh.mesh = AssetLibrary::Instance().LoadMesh(mesh.meshReference);
        
        if (!mesh.mesh) {
            std::cout << "[Serializer] Warning: Failed to load mesh from asset reference, falling back to name-based system" << std::endl;
        }
    }
    
    // Fallback to the old name-based system
    if (!mesh.mesh && data.contains("meshName")) {
        mesh.MeshName = data["meshName"];
        
        // Load the actual mesh from StandardMeshManager based on the name
        if (mesh.MeshName == "Cube" || mesh.MeshName == "DebugCube") {
            mesh.mesh = StandardMeshManager::Instance().GetCubeMesh();
        } else if (mesh.MeshName == "Sphere") {
            mesh.mesh = StandardMeshManager::Instance().GetSphereMesh();
        } else if (mesh.MeshName == "Plane") {
            mesh.mesh = StandardMeshManager::Instance().GetPlaneMesh();
        } else if (mesh.MeshName == "ImageQuad") {
            mesh.mesh = StandardMeshManager::Instance().GetPlaneMesh();
        } else {
            // For other mesh names, try to get a default cube mesh as fallback
            std::cout << "[Serializer] Warning: Unknown mesh name '" << mesh.MeshName << "', using default cube mesh" << std::endl;
            mesh.mesh = StandardMeshManager::Instance().GetCubeMesh();
        }
    }
    
    if (data.contains("materialName")) {
        // Load material from MaterialManager
        mesh.material = MaterialManager::Instance().CreateDefaultPBRMaterial();
    } else {
        // Set default material if none specified
        mesh.material = MaterialManager::Instance().CreateDefaultPBRMaterial();
    }
}

json Serializer::SerializeLight(const LightComponent& light) {
    json data;
    data["type"] = static_cast<int>(light.Type);
    data["color"] = SerializeVec3(light.Color);
    data["intensity"] = light.Intensity;
    return data;
}

void Serializer::DeserializeLight(const json& data, LightComponent& light) {
    if (data.contains("type")) light.Type = static_cast<LightType>(data["type"]);
    if (data.contains("color")) light.Color = DeserializeVec3(data["color"]);
    if (data.contains("intensity")) light.Intensity = data["intensity"];
}

json Serializer::SerializeCollider(const ColliderComponent& collider) {
    json data;
    data["shapeType"] = static_cast<int>(collider.ShapeType);
    data["offset"] = SerializeVec3(collider.Offset);
    data["size"] = SerializeVec3(collider.Size);
    data["radius"] = collider.Radius;
    data["height"] = collider.Height;
    data["meshPath"] = collider.MeshPath;
    data["isTrigger"] = collider.IsTrigger;
    return data;
}

void Serializer::DeserializeCollider(const json& data, ColliderComponent& collider) {
    if (data.contains("shapeType")) collider.ShapeType = static_cast<ColliderShape>(data["shapeType"]);
    if (data.contains("offset")) collider.Offset = DeserializeVec3(data["offset"]);
    if (data.contains("size")) collider.Size = DeserializeVec3(data["size"]);
    if (data.contains("radius")) collider.Radius = data["radius"];
    if (data.contains("height")) collider.Height = data["height"];
    if (data.contains("meshPath")) collider.MeshPath = data["meshPath"];
    if (data.contains("isTrigger")) collider.IsTrigger = data["isTrigger"];
}

// RigidBody serialization
json Serializer::SerializeRigidBody(const RigidBodyComponent& rigidbody) {
    json data;
    data["mass"] = rigidbody.Mass;
    data["friction"] = rigidbody.Friction;
    data["restitution"] = rigidbody.Restitution;
    data["useGravity"] = rigidbody.UseGravity;
    data["isKinematic"] = rigidbody.IsKinematic;
    data["linearVelocity"] = SerializeVec3(rigidbody.LinearVelocity);
    data["angularVelocity"] = SerializeVec3(rigidbody.AngularVelocity);
    return data;
}

void Serializer::DeserializeRigidBody(const json& data, RigidBodyComponent& rigidbody) {
    if (data.contains("mass")) rigidbody.Mass = data["mass"];
    if (data.contains("friction")) rigidbody.Friction = data["friction"];
    if (data.contains("restitution")) rigidbody.Restitution = data["restitution"];
    if (data.contains("useGravity")) rigidbody.UseGravity = data["useGravity"];
    if (data.contains("isKinematic")) rigidbody.IsKinematic = data["isKinematic"];
    if (data.contains("linearVelocity")) rigidbody.LinearVelocity = DeserializeVec3(data["linearVelocity"]);
    if (data.contains("angularVelocity")) rigidbody.AngularVelocity = DeserializeVec3(data["angularVelocity"]);
}

// StaticBody serialization
json Serializer::SerializeStaticBody(const StaticBodyComponent& staticbody) {
    json data;
    data["friction"] = staticbody.Friction;
    data["restitution"] = staticbody.Restitution;
    return data;
}

void Serializer::DeserializeStaticBody(const json& data, StaticBodyComponent& staticbody) {
    if (data.contains("friction")) staticbody.Friction = data["friction"];
    if (data.contains("restitution")) staticbody.Restitution = data["restitution"];
}

// Camera serialization
json Serializer::SerializeCamera(const CameraComponent& camera) {
    json data;
    data["active"] = camera.Active;
    data["priority"] = camera.priority;
    data["fov"] = camera.FieldOfView;
    data["nearClip"] = camera.NearClip;
    data["farClip"] = camera.FarClip;
    data["isPerspective"] = camera.IsPerspective;
    return data;
}

void Serializer::DeserializeCamera(const json& data, CameraComponent& camera) {
    if (data.contains("active")) camera.Active = data["active"];
    if (data.contains("priority")) camera.priority = data["priority"];
    if (data.contains("fov")) camera.FieldOfView = data["fov"];
    if (data.contains("nearClip")) camera.NearClip = data["nearClip"];
    if (data.contains("farClip")) camera.FarClip = data["farClip"];
    if (data.contains("isPerspective")) camera.IsPerspective = data["isPerspective"];
}

// Terrain serialization (only essentials to reconstruct deterministically)
json Serializer::SerializeTerrain(const TerrainComponent& terrain) {
    json data;
    data["mode"] = terrain.Mode;
    data["size"] = terrain.Size;
    data["paintMode"] = terrain.PaintMode;
    // Persist heightmap raw bytes as array (compact); could be optimized later
    // Build array without using reserve (not supported by nlohmann::json)
    json heightArray = json::array();
    heightArray.get_ptr<json::array_t*>()->reserve(terrain.HeightMap.size());
    for (uint8_t v : terrain.HeightMap) heightArray.push_back(v);
    data["heightMap"] = std::move(heightArray);
    return data;
}

void Serializer::DeserializeTerrain(const json& data, TerrainComponent& terrain) {
    if (data.contains("mode")) terrain.Mode = data["mode"];
    if (data.contains("size")) terrain.Size = data["size"];
    if (data.contains("paintMode")) terrain.PaintMode = data["paintMode"];
    if (data.contains("heightMap") && data["heightMap"].is_array()) {
        const auto& arr = data["heightMap"];
        terrain.HeightMap.resize(arr.size());
        for (size_t i = 0; i < arr.size(); ++i) terrain.HeightMap[i] = arr[i];
        terrain.Dirty = true;
    }
}

// Particle emitter serialization
json Serializer::SerializeParticleEmitter(const ParticleEmitterComponent& emitter) {
    json data;
    data["enabled"] = emitter.Enabled;
    data["maxParticles"] = emitter.MaxParticles;
    // Minimal uniforms to ensure stable replay; extend as needed
    data["particlesPerSecond"] = emitter.Uniforms.m_particlesPerSecond;
    data["blendMode"] = emitter.Uniforms.m_blendMode;
    if (!emitter.SpritePath.empty()) data["spritePath"] = emitter.SpritePath;
    // Optional: sprite is an engine-created resource; omit for now
    return data;
}

void Serializer::DeserializeParticleEmitter(const json& data, ParticleEmitterComponent& emitter) {
    if (data.contains("enabled")) emitter.Enabled = data["enabled"];
    if (data.contains("maxParticles")) emitter.MaxParticles = data["maxParticles"];
    if (data.contains("particlesPerSecond")) emitter.Uniforms.m_particlesPerSecond = data["particlesPerSecond"];
    if (data.contains("blendMode")) emitter.Uniforms.m_blendMode = data["blendMode"];
    if (data.contains("spritePath")) emitter.SpritePath = data["spritePath"];
}

// UI serialization
json Serializer::SerializeCanvas(const CanvasComponent& canvas) {
    json data;
    data["width"] = canvas.Width;
    data["height"] = canvas.Height;
    data["dpiScale"] = canvas.DPIScale;
    data["space"] = static_cast<int>(canvas.Space);
    data["sortOrder"] = canvas.SortOrder;
    data["blockSceneInput"] = canvas.BlockSceneInput;
    return data;
}

void Serializer::DeserializeCanvas(const json& data, CanvasComponent& canvas) {
    if (data.contains("width")) canvas.Width = data["width"];
    if (data.contains("height")) canvas.Height = data["height"];
    if (data.contains("dpiScale")) canvas.DPIScale = data["dpiScale"];
    if (data.contains("space")) canvas.Space = static_cast<CanvasComponent::RenderSpace>(data["space"]);
    if (data.contains("sortOrder")) canvas.SortOrder = data["sortOrder"];
    if (data.contains("blockSceneInput")) canvas.BlockSceneInput = data["blockSceneInput"];
}

json Serializer::SerializePanel(const PanelComponent& panel) {
    json data;
    data["position"] = { panel.Position.x, panel.Position.y };
    data["size"] = { panel.Size.x, panel.Size.y };
    data["pivot"] = { panel.Pivot.x, panel.Pivot.y };
    data["rotation"] = panel.Rotation;
    data["texture"] = panel.Texture;
    data["uvRect"] = { panel.UVRect.x, panel.UVRect.y, panel.UVRect.z, panel.UVRect.w };
    data["tintColor"] = { panel.TintColor.r, panel.TintColor.g, panel.TintColor.b, panel.TintColor.a };
    data["opacity"] = panel.Opacity;
    data["visible"] = panel.Visible;
    data["zOrder"] = panel.ZOrder;
    return data;
}

void Serializer::DeserializePanel(const json& data, PanelComponent& panel) {
    if (data.contains("position") && data["position"].is_array() && data["position"].size() == 2) {
        panel.Position.x = data["position"][0];
        panel.Position.y = data["position"][1];
    }
    if (data.contains("size") && data["size"].is_array() && data["size"].size() == 2) {
        panel.Size.x = data["size"][0];
        panel.Size.y = data["size"][1];
    }
    if (data.contains("pivot") && data["pivot"].is_array() && data["pivot"].size() == 2) {
        panel.Pivot.x = data["pivot"][0];
        panel.Pivot.y = data["pivot"][1];
    }
    if (data.contains("rotation")) panel.Rotation = data["rotation"];
    if (data.contains("texture")) data["texture"].get_to(panel.Texture);
    if (data.contains("uvRect") && data["uvRect"].is_array() && data["uvRect"].size() == 4) {
        panel.UVRect.x = data["uvRect"][0];
        panel.UVRect.y = data["uvRect"][1];
        panel.UVRect.z = data["uvRect"][2];
        panel.UVRect.w = data["uvRect"][3];
    }
    if (data.contains("tintColor") && data["tintColor"].is_array() && data["tintColor"].size() == 4) {
        panel.TintColor.r = data["tintColor"][0];
        panel.TintColor.g = data["tintColor"][1];
        panel.TintColor.b = data["tintColor"][2];
        panel.TintColor.a = data["tintColor"][3];
    }
    if (data.contains("opacity")) panel.Opacity = data["opacity"];
    if (data.contains("visible")) panel.Visible = data["visible"];
    if (data.contains("zOrder")) panel.ZOrder = data["zOrder"];
}

json Serializer::SerializeButton(const ButtonComponent& button) {
    json data;
    data["interactable"] = button.Interactable;
    data["toggle"] = button.Toggle;
    data["toggled"] = button.Toggled;
    data["normalTint"] = { button.NormalTint.r, button.NormalTint.g, button.NormalTint.b, button.NormalTint.a };
    data["hoverTint"] = { button.HoverTint.r, button.HoverTint.g, button.HoverTint.b, button.HoverTint.a };
    data["pressedTint"] = { button.PressedTint.r, button.PressedTint.g, button.PressedTint.b, button.PressedTint.a };
    data["hoverSound"] = button.HoverSound;
    data["clickSound"] = button.ClickSound;
    return data;
}

void Serializer::DeserializeButton(const json& data, ButtonComponent& button) {
    if (data.contains("interactable")) button.Interactable = data["interactable"];
    if (data.contains("toggle")) button.Toggle = data["toggle"];
    if (data.contains("toggled")) button.Toggled = data["toggled"];
    if (data.contains("normalTint") && data["normalTint"].is_array() && data["normalTint"].size() == 4) {
        button.NormalTint.r = data["normalTint"][0];
        button.NormalTint.g = data["normalTint"][1];
        button.NormalTint.b = data["normalTint"][2];
        button.NormalTint.a = data["normalTint"][3];
    }
    if (data.contains("hoverTint") && data["hoverTint"].is_array() && data["hoverTint"].size() == 4) {
        button.HoverTint.r = data["hoverTint"][0];
        button.HoverTint.g = data["hoverTint"][1];
        button.HoverTint.b = data["hoverTint"][2];
        button.HoverTint.a = data["hoverTint"][3];
    }
    if (data.contains("pressedTint") && data["pressedTint"].is_array() && data["pressedTint"].size() == 4) {
        button.PressedTint.r = data["pressedTint"][0];
        button.PressedTint.g = data["pressedTint"][1];
        button.PressedTint.b = data["pressedTint"][2];
        button.PressedTint.a = data["pressedTint"][3];
    }
    if (data.contains("hoverSound")) data["hoverSound"].get_to(button.HoverSound);
    if (data.contains("clickSound")) data["clickSound"].get_to(button.ClickSound);
}

json Serializer::SerializeScripts(const std::vector<ScriptInstance>& scripts) {
    json scriptArray = json::array();
    for (const auto& script : scripts) {
        json scriptData;
        scriptData["className"] = script.ClassName;
        // TODO: Add script property serialization when reflection system is implemented
        scriptArray.push_back(scriptData);
    }
    return scriptArray;
}

void Serializer::DeserializeScripts(const json& data, std::vector<ScriptInstance>& scripts) {
    scripts.clear();
    if (data.is_array()) {
        for (const auto& scriptData : data) {
            if (scriptData.contains("className")) {
                ScriptInstance instance;
                instance.ClassName = scriptData["className"];
                
                // Create the script instance
                auto created = ScriptSystem::Instance().Create(instance.ClassName);
                if (created) {
                    instance.Instance = created;
                    scripts.push_back(instance);
                } else {
                    std::cerr << "[Serializer] Failed to create script of type '" << instance.ClassName << "'\n";
                }
            }
        }
    }
}

// Entity serialization
json Serializer::SerializeEntity(EntityID id, Scene& scene) {
   EntityData* entityData = scene.GetEntityData(id);  // <-- This is the correct call
   if (!entityData) return json{};

   json data;
   data["id"] = id;
   data["name"] = entityData->Name;
   data["layer"] = entityData->Layer;
   data["tag"] = entityData->Tag;
   data["parent"] = entityData->Parent;
   data["children"] = entityData->Children;

   // Serialize components
   data["transform"] = SerializeTransform(entityData->Transform);

   if (entityData->Mesh) {
      data["mesh"] = SerializeMesh(*entityData->Mesh);
      }

   if (entityData->Light) {
      data["light"] = SerializeLight(*entityData->Light);
      }

   if (entityData->Collider) {
      data["collider"] = SerializeCollider(*entityData->Collider);
      }

   if (entityData->RigidBody) {
      data["rigidbody"] = SerializeRigidBody(*entityData->RigidBody);
      }

   if (entityData->StaticBody) {
      data["staticbody"] = SerializeStaticBody(*entityData->StaticBody);
      }

   // Serialize scripts
   if (!entityData->Scripts.empty()) {
      data["scripts"] = SerializeScripts(entityData->Scripts);
      }

   if (entityData->Camera) {
       data["camera"] = SerializeCamera(*entityData->Camera);
   }
   if (entityData->Terrain) {
       data["terrain"] = SerializeTerrain(*entityData->Terrain);
   }
   if (entityData->Emitter) {
       data["emitter"] = SerializeParticleEmitter(*entityData->Emitter);
   }

   // UI Components
   if (entityData->Canvas) {
       data["canvas"] = SerializeCanvas(*entityData->Canvas);
   }
   if (entityData->Panel) {
       data["panel"] = SerializePanel(*entityData->Panel);
   }
   if (entityData->Button) {
       data["button"] = SerializeButton(*entityData->Button);
   }

   return data;
   }


EntityID Serializer::DeserializeEntity(const json& data, Scene& scene) {
    if (!data.contains("name")) return 0;

    std::string name = data["name"];
    Entity entity = scene.CreateEntity(name);
    EntityID id = entity.GetID();
    
    auto* entityData = scene.GetEntityData(id);
    if (!entityData) return 0;

    // Deserialize basic properties
    if (data.contains("layer")) entityData->Layer = data["layer"];
    if (data.contains("tag")) entityData->Tag = data["tag"];
    if (data.contains("parent")) entityData->Parent = data["parent"];
    if (data.contains("children")) {
       entityData->Children.clear();
       for (const auto& child : data["children"]) {
          entityData->Children.push_back(child.get<EntityID>());
          }
       }


    // Deserialize transform
    if (data.contains("transform")) {
        DeserializeTransform(data["transform"], entityData->Transform);
    }

    // Deserialize components
    if (data.contains("mesh")) {
        entityData->Mesh = new MeshComponent();
        DeserializeMesh(data["mesh"], *entityData->Mesh);
    }

    if (data.contains("light")) {
        entityData->Light = new LightComponent();
        DeserializeLight(data["light"], *entityData->Light);
    }

    if (data.contains("collider")) {
        entityData->Collider = new ColliderComponent();
        DeserializeCollider(data["collider"], *entityData->Collider);
    }

    if (data.contains("rigidbody")) {
        entityData->RigidBody = new RigidBodyComponent();
        DeserializeRigidBody(data["rigidbody"], *entityData->RigidBody);
    }

    if (data.contains("staticbody")) {
        entityData->StaticBody = new StaticBodyComponent();
        DeserializeStaticBody(data["staticbody"], *entityData->StaticBody);
    }

    if (data.contains("camera")) {
        entityData->Camera = new CameraComponent();
        DeserializeCamera(data["camera"], *entityData->Camera);
    }
    if (data.contains("terrain")) {
        entityData->Terrain = new TerrainComponent();
        DeserializeTerrain(data["terrain"], *entityData->Terrain);
    }
    if (data.contains("emitter")) {
        entityData->Emitter = new ParticleEmitterComponent();
        DeserializeParticleEmitter(data["emitter"], *entityData->Emitter);
    }

    // UI Components
    if (data.contains("canvas")) {
        entityData->Canvas = new CanvasComponent();
        DeserializeCanvas(data["canvas"], *entityData->Canvas);
    }
    if (data.contains("panel")) {
        entityData->Panel = new PanelComponent();
        DeserializePanel(data["panel"], *entityData->Panel);
    }
    if (data.contains("button")) {
        entityData->Button = new ButtonComponent();
        DeserializeButton(data["button"], *entityData->Button);
    }

    // Deserialize scripts
    if (data.contains("scripts")) {
        DeserializeScripts(data["scripts"], entityData->Scripts);
    }

    return id;
}

// Scene serialization
json Serializer::SerializeScene( Scene& scene) {
    json sceneData;
    sceneData["version"] = "1.0";
    sceneData["entities"] = json::array();

    for (const auto& entity : scene.GetEntities()) {
        json entityData = SerializeEntity(entity.GetID(), scene);
        if (!entityData.empty()) {
            sceneData["entities"].push_back(entityData);
        }
    }

    return sceneData;
}

bool Serializer::DeserializeScene(const json& data, Scene& scene) {
    if (!data.contains("entities")) return false;

    // Clear existing scene by removing all entities
    std::vector<EntityID> entitiesToRemove;
    for (const auto& entity : scene.GetEntities()) {
        entitiesToRemove.push_back(entity.GetID());
    }
    
    for (EntityID id : entitiesToRemove) {
        scene.RemoveEntity(id);
    }
    
    // First pass: Create all entities
    std::unordered_map<EntityID, EntityID> idMapping; // old ID -> new ID
    
    for (const auto& entityData : data["entities"]) {
        // Preserve names exactly as authored. Create with base name without suffixing.
        EntityID newId = 0;
        if (entityData.contains("name")) {
            // Create a temporary entity and immediately set the exact name
            Entity temp = scene.CreateEntity(entityData["name"]);
            newId = temp.GetID();
            auto* ed = scene.GetEntityData(newId);
            if (ed) ed->Name = entityData["name"]; // avoid auto-suffix pattern
            // Fill remaining fields by deserializing into this entity
            // Reuse DeserializeEntity by overriding name to avoid creating a second entity
            // We'll simulate by building a new json without name to skip creation
            json copy = entityData;
            copy.erase("name");
            // Write id to ensure relationships mapping still works
            // Then call DeserializeEntity on a synthetic structure that keeps properties
            // but our DeserializeEntity currently always creates a new entity.
            // Instead, manually apply properties here for the created entity:
            if (copy.contains("layer")) ed->Layer = copy["layer"];
            if (copy.contains("tag")) ed->Tag = copy["tag"];
            if (copy.contains("parent")) ed->Parent = copy["parent"];
            if (copy.contains("children")) {
                ed->Children.clear();
                for (const auto& child : copy["children"]) ed->Children.push_back(child.get<EntityID>());
            }
            if (copy.contains("transform")) DeserializeTransform(copy["transform"], ed->Transform);
            if (copy.contains("mesh")) { ed->Mesh = new MeshComponent(); DeserializeMesh(copy["mesh"], *ed->Mesh); }
            if (copy.contains("light")) { ed->Light = new LightComponent(); DeserializeLight(copy["light"], *ed->Light); }
            if (copy.contains("collider")) { ed->Collider = new ColliderComponent(); DeserializeCollider(copy["collider"], *ed->Collider); }
            if (copy.contains("rigidbody")) { ed->RigidBody = new RigidBodyComponent(); DeserializeRigidBody(copy["rigidbody"], *ed->RigidBody); }
            if (copy.contains("staticbody")) { ed->StaticBody = new StaticBodyComponent(); DeserializeStaticBody(copy["staticbody"], *ed->StaticBody); }
            if (copy.contains("camera")) { ed->Camera = new CameraComponent(); DeserializeCamera(copy["camera"], *ed->Camera); }
            if (copy.contains("terrain")) { ed->Terrain = new TerrainComponent(); DeserializeTerrain(copy["terrain"], *ed->Terrain); }
            if (copy.contains("emitter")) { ed->Emitter = new ParticleEmitterComponent(); DeserializeParticleEmitter(copy["emitter"], *ed->Emitter); }
            if (copy.contains("canvas")) { ed->Canvas = new CanvasComponent(); DeserializeCanvas(copy["canvas"], *ed->Canvas); }
            if (copy.contains("panel")) { ed->Panel = new PanelComponent(); DeserializePanel(copy["panel"], *ed->Panel); }
            if (copy.contains("button")) { ed->Button = new ButtonComponent(); DeserializeButton(copy["button"], *ed->Button); }
            if (copy.contains("scripts")) { DeserializeScripts(copy["scripts"], ed->Scripts); }
        } else {
            newId = DeserializeEntity(entityData, scene);
        }
        if (newId != 0 && entityData.contains("id")) {
            EntityID oldId = entityData["id"];
            idMapping[oldId] = newId;
        }
    }

    // Reset children vectors to avoid duplicates, then fix up parent-child relationships
    for (const auto& [oldId, newId] : idMapping) {
        if (auto* ed = scene.GetEntityData(newId)) ed->Children.clear();
    }
    // Second pass: Fix up parent-child relationships
    for (const auto& entityData : data["entities"]) {
        if (entityData.contains("id") && entityData.contains("parent")) {
            EntityID oldId = entityData["id"];
            EntityID oldParent = entityData["parent"];
            
            if (idMapping.find(oldId) != idMapping.end() && 
                idMapping.find(oldParent) != idMapping.end()) {
                scene.SetParent(idMapping[oldId], idMapping[oldParent]);
            }
        }
    }

    // Ensure transforms are dirty and updated after load
    for (const auto& entity : scene.GetEntities()) {
        scene.MarkTransformDirty(entity.GetID());
    }
    scene.UpdateTransforms();

    return true;
}

bool Serializer::SaveSceneToFile(Scene& scene, const std::string& filepath) {
    try {
        json sceneData = SerializeScene(scene);
        
        // Ensure directory exists
        fs::path path(filepath);
        fs::create_directories(path.parent_path());
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[Serializer] Failed to open file for writing: " << filepath << std::endl;
            return false;
        }
        
        file << sceneData.dump(4); // Pretty print with 4 spaces
        file.close();
        
        std::cout << "[Serializer] Scene saved to: " << filepath << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[Serializer] Error saving scene: " << e.what() << std::endl;
        return false;
    }
}

bool Serializer::LoadSceneFromFile(const std::string& filepath, Scene& scene) {
    try {
        if (!fs::exists(filepath)) {
            std::cerr << "[Serializer] Scene file does not exist: " << filepath << std::endl;
            return false;
        }
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[Serializer] Failed to open scene file: " << filepath << std::endl;
            return false;
        }
        
        json sceneData;
        file >> sceneData;
        file.close();
        
        bool success = DeserializeScene(sceneData, scene);
        if (success) {
            std::cout << "[Serializer] Scene loaded from: " << filepath << std::endl;
        }
        return success;
    }
    catch (const std::exception& e) {
        std::cerr << "[Serializer] Error loading scene: " << e.what() << std::endl;
        return false;
    }
}

// Prefab serialization
json Serializer::SerializePrefab(const EntityData& entityData, Scene& scene) {
    json prefabData;
    prefabData["version"] = "1.0";
    prefabData["type"] = "prefab";
    
    // Create a temporary entity to serialize
    // Note: This is a simplified approach - in a full implementation,
    // we might want to serialize the EntityData directly
    json entityJson;
    entityJson["name"] = entityData.Name;
    entityJson["layer"] = entityData.Layer;
    entityJson["tag"] = entityData.Tag;
    entityJson["transform"] = SerializeTransform(entityData.Transform);
    
    if (entityData.Mesh) {
        entityJson["mesh"] = SerializeMesh(*entityData.Mesh);
    }
    
    if (entityData.Light) {
        entityJson["light"] = SerializeLight(*entityData.Light);
    }
    
    if (entityData.Collider) {
        entityJson["collider"] = SerializeCollider(*entityData.Collider);
    }

    if (!entityData.Scripts.empty()) {
        entityJson["scripts"] = SerializeScripts(entityData.Scripts);
    }

    prefabData["entity"] = entityJson;
    return prefabData;
}

bool Serializer::DeserializePrefab(const json& data, EntityData& entityData, Scene& scene) {
    if (!data.contains("entity")) return false;

    const json& entityJson = data["entity"];
    
    // Reset the entity data
    entityData = EntityData{};
    
    // Deserialize basic properties
    if (entityJson.contains("name")) entityData.Name = entityJson["name"];
    if (entityJson.contains("layer")) entityData.Layer = entityJson["layer"];
    if (entityJson.contains("tag")) entityData.Tag = entityJson["tag"];

    // Deserialize transform
    if (entityJson.contains("transform")) {
        DeserializeTransform(entityJson["transform"], entityData.Transform);
    }

    // Deserialize components
    if (entityJson.contains("mesh")) {
        entityData.Mesh = new MeshComponent();
        DeserializeMesh(entityJson["mesh"], *entityData.Mesh);
    }

    if (entityJson.contains("light")) {
        entityData.Light = new LightComponent();
        DeserializeLight(entityJson["light"], *entityData.Light);
    }

    if (entityJson.contains("collider")) {
        entityData.Collider = new ColliderComponent();
        DeserializeCollider(entityJson["collider"], *entityData.Collider);
    }

    // Deserialize scripts
    if (entityJson.contains("scripts")) {
        DeserializeScripts(entityJson["scripts"], entityData.Scripts);
    }

    return true;
}

bool Serializer::SavePrefabToFile(const EntityData& entityData, Scene& scene, const std::string& filepath) {
    try {
        json prefabData = SerializePrefab(entityData, scene);
        
        // Ensure directory exists
        fs::path path(filepath);
        fs::create_directories(path.parent_path());
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[Serializer] Failed to open file for writing: " << filepath << std::endl;
            return false;
        }
        
        file << prefabData.dump(4); // Pretty print with 4 spaces
        file.close();
        
        std::cout << "[Serializer] Prefab saved to: " << filepath << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[Serializer] Error saving prefab: " << e.what() << std::endl;
        return false;
    }
}

bool Serializer::LoadPrefabFromFile(const std::string& filepath, EntityData& entityData, Scene& scene) {
    try {
        if (!fs::exists(filepath)) {
            std::cerr << "[Serializer] Prefab file does not exist: " << filepath << std::endl;
            return false;
        }
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[Serializer] Failed to open prefab file: " << filepath << std::endl;
            return false;
        }
        
        json prefabData;
        file >> prefabData;
        file.close();
        
        bool success = DeserializePrefab(prefabData, entityData, scene);
        if (success) {
            std::cout << "[Serializer] Prefab loaded from: " << filepath << std::endl;
        }
        return success;
    }
    catch (const std::exception& e) {
        std::cerr << "[Serializer] Error loading prefab: " << e.what() << std::endl;
        return false;
    }
}