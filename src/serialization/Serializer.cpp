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
#include "animation/AnimationPlayerComponent.h"
#include "io/FileSystem.h"
#include <editor/Project.h>

namespace fs = std::filesystem;

// Heuristic: Determine if an entity is the root of an imported model. If so, try to infer model path
static bool IsImportedModelRoot(Scene& scene, EntityID id, std::string& outModelPath, ClaymoreGUID& outGuid) {
    auto* ed = scene.GetEntityData(id);
    if (!ed) return false;
    // Root should not have its own mesh
    if (ed->Mesh) return false;
    // Search descendants for the first mesh that comes from a non-primitive model
    std::function<bool(EntityID)> dfs = [&](EntityID e)->bool{
        auto* cd = scene.GetEntityData(e);
        if (!cd) return false;
        if (cd->Mesh && cd->Mesh->meshReference.IsValid()) {
            ClaymoreGUID g = cd->Mesh->meshReference.guid;
            std::string p = AssetLibrary::Instance().GetPathForGUID(g);
            if (!p.empty()) {
                outModelPath = p; outGuid = g; return true;
            }
        }
        for (EntityID c : cd->Children) if (dfs(c)) return true;
        return false;
    };
    for (EntityID c : ed->Children) if (dfs(c)) return true;
    return false;
}

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
    // Persist model/mesh location hints for robust reloads
    if (mesh.meshReference.IsValid()) {
        std::string p = AssetLibrary::Instance().GetPathForGUID(mesh.meshReference.guid);
        if (!p.empty()) data["meshPath"] = p;
        data["fileID"] = mesh.meshReference.fileID;
    }
    data["uniqueMaterial"] = mesh.UniqueMaterial;

    if (mesh.material) {
        data["materialName"] = mesh.material->GetName();
        // Store material properties if it's a PBR material
        if (auto pbr = std::dynamic_pointer_cast<PBRMaterial>(mesh.material)) {
            data["materialType"] = "PBR";
            // Persist texture source paths for unique materials
            if (mesh.UniqueMaterial) {
                if (!pbr->GetAlbedoPath().empty()) data["mat_albedoPath"] = pbr->GetAlbedoPath();
                if (!pbr->GetMetallicRoughnessPath().empty()) data["mat_mrPath"] = pbr->GetMetallicRoughnessPath();
                if (!pbr->GetNormalPath().empty()) data["mat_normalPath"] = pbr->GetNormalPath();
            }
        }
    }

    // Persist PropertyBlock overrides
    if (!mesh.PropertyBlock.Vec4Uniforms.empty()) {
        json jvec = json::object();
        for (const auto& kv : mesh.PropertyBlock.Vec4Uniforms) {
            jvec[kv.first] = { kv.second.x, kv.second.y, kv.second.z, kv.second.w };
        }
        data["propertyBlockVec4"] = std::move(jvec);
    }
    if (!mesh.PropertyBlockTexturePaths.empty()) {
        // store texture override paths by uniform name
        json jtex = json::object();
        for (const auto& kv : mesh.PropertyBlockTexturePaths) {
            jtex[kv.first] = kv.second;
        }
        data["propertyBlockTextures"] = std::move(jtex);
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
            // Last attempt: if we can resolve GUID->path at runtime, try direct path model load
            std::string p = AssetLibrary::Instance().GetPathForGUID(mesh.meshReference.guid);
            if (!p.empty()) {
                Model mdl = ModelLoader::LoadModel(p);
                if (!mdl.Meshes.empty()) {
                    int idx = std::max(0, mesh.meshReference.fileID);
                    if (idx < (int)mdl.Meshes.size()) mesh.mesh = mdl.Meshes[idx];
                    else mesh.mesh = mdl.Meshes[0];
                }
            }
            // Additional fallback: legacy scenes with absolute or project paths recorded
            if (!mesh.mesh && data.contains("meshPath")) {
                std::string absOrRel = data["meshPath"].get<std::string>();
                std::string norm = absOrRel; for (char &c : norm) if (c=='\\') c='/';
                ClaymoreGUID g = AssetLibrary::Instance().GetGUIDForPath(norm);
                if (g.high != 0 || g.low != 0) {
                    AssetReference tmp(g, mesh.meshReference.fileID, (int)AssetType::Mesh);
                    mesh.mesh = AssetLibrary::Instance().LoadMesh(tmp);
                }
            }
        }
    }
    
    // Fallback to the old name-based system and primitive GUIDs
    if (!mesh.mesh) {
        // Primitive GUID system
        if (mesh.meshReference.guid == AssetReference::CreatePrimitive("").guid) {
            // fileID indicates which primitive
            switch (mesh.meshReference.fileID) {
                case 0: mesh.mesh = StandardMeshManager::Instance().GetCubeMesh(); break;
                case 1: mesh.mesh = StandardMeshManager::Instance().GetSphereMesh(); break;
                case 2: mesh.mesh = StandardMeshManager::Instance().GetPlaneMesh(); break;
                default: mesh.mesh = StandardMeshManager::Instance().GetCubeMesh(); break;
            }
        }
        // Name-based
        if (!mesh.mesh && data.contains("meshName")) {
            mesh.MeshName = data["meshName"];
            if (mesh.MeshName == "Cube" || mesh.MeshName == "DebugCube") {
                mesh.mesh = StandardMeshManager::Instance().GetCubeMesh();
            } else if (mesh.MeshName == "Sphere") {
                mesh.mesh = StandardMeshManager::Instance().GetSphereMesh();
            } else if (mesh.MeshName == "Plane") {
                mesh.mesh = StandardMeshManager::Instance().GetPlaneMesh();
            } else if (mesh.MeshName == "ImageQuad") {
                mesh.mesh = StandardMeshManager::Instance().GetPlaneMesh();
            } else {
                std::cout << "[Serializer] Warning: Unknown mesh name '" << mesh.MeshName << "', using default cube mesh" << std::endl;
                mesh.mesh = StandardMeshManager::Instance().GetCubeMesh();
            }
        }
    }
    
    // Material: if not already set by caller (e.g., skinned detection), assign default PBR
    if (!mesh.material) {
        mesh.material = MaterialManager::Instance().CreateDefaultPBRMaterial();
    }

    // If the material is unique and we have texture source paths, restore them
    if (mesh.UniqueMaterial) {
        if (auto pbr = std::dynamic_pointer_cast<PBRMaterial>(mesh.material)) {
            auto applyTex = [&](const char* key, auto setter){
                if (!data.contains(key)) return;
                std::string path = data[key].get<std::string>();
                if (path.empty()) return;
                // Allow virtual or absolute; route via FileSystem-enabled loader
                bgfx::TextureHandle t = TextureLoader::Load2D(path);
                if (bgfx::isValid(t)) setter(t), setter(path);
            };
            if (data.contains("mat_albedoPath")) { pbr->SetAlbedoTexture(TextureLoader::Load2D(data["mat_albedoPath"].get<std::string>())); pbr->SetAlbedoTextureFromPath(data["mat_albedoPath"].get<std::string>()); }
            if (data.contains("mat_mrPath")) { pbr->SetMetallicRoughnessTexture(TextureLoader::Load2D(data["mat_mrPath"].get<std::string>())); pbr->SetMetallicRoughnessTextureFromPath(data["mat_mrPath"].get<std::string>()); }
            if (data.contains("mat_normalPath")) { pbr->SetNormalTexture(TextureLoader::Load2D(data["mat_normalPath"].get<std::string>())); pbr->SetNormalTextureFromPath(data["mat_normalPath"].get<std::string>()); }
        }
    }

    // Unique material toggle
    if (data.contains("uniqueMaterial")) {
        mesh.UniqueMaterial = data["uniqueMaterial"].get<bool>();
    }

    // PropertyBlock overrides
    mesh.PropertyBlock.Clear();
    if (data.contains("propertyBlockVec4") && data["propertyBlockVec4"].is_object()) {
        for (auto it = data["propertyBlockVec4"].begin(); it != data["propertyBlockVec4"].end(); ++it) {
            const auto& arr = it.value();
            if (arr.is_array() && arr.size() == 4) {
                mesh.PropertyBlock.Vec4Uniforms[it.key()] = glm::vec4(arr[0], arr[1], arr[2], arr[3]);
            }
        }
    }
    mesh.PropertyBlockTexturePaths.clear();
    if (data.contains("propertyBlockTextures") && data["propertyBlockTextures"].is_object()) {
        for (auto it = data["propertyBlockTextures"].begin(); it != data["propertyBlockTextures"].end(); ++it) {
            const std::string uniform = it.key();
            const std::string path = it.value().get<std::string>();
            mesh.PropertyBlockTexturePaths[uniform] = path;
            bgfx::TextureHandle tex = TextureLoader::Load2D(path);
            if (bgfx::isValid(tex)) {
                mesh.PropertyBlock.Textures[uniform] = tex;
            }
        }
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
    data["anchorEnabled"] = panel.AnchorEnabled;
    data["anchor"] = (int)panel.Anchor;
    data["anchorOffset"] = { panel.AnchorOffset.x, panel.AnchorOffset.y };
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
    if (data.contains("anchorEnabled")) panel.AnchorEnabled = data["anchorEnabled"];
    if (data.contains("anchor")) panel.Anchor = (UIAnchorPreset)((int)data["anchor"]);
    if (data.contains("anchorOffset") && data["anchorOffset"].is_array() && data["anchorOffset"].size() == 2) {
        panel.AnchorOffset.x = data["anchorOffset"][0];
        panel.AnchorOffset.y = data["anchorOffset"][1];
    }
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

// ---------------- Animator (AnimationPlayerComponent) ----------------
json Serializer::SerializeAnimator(const cm::animation::AnimationPlayerComponent& a)
{
    json j;
    j["mode"] = (a.AnimatorMode == cm::animation::AnimationPlayerComponent::Mode::ControllerAnimated) ? "controller" : "player";
    j["playbackSpeed"] = a.PlaybackSpeed;
    j["rootMotion"] = (int)a.RootMotion;
    j["controllerPath"] = a.ControllerPath;
    j["singleClipPath"] = a.SingleClipPath;
    j["playOnStart"] = a.PlayOnStart;
    j["loop"] = (!a.ActiveStates.empty() ? a.ActiveStates.front().Loop : true);
    return j;
}

void Serializer::DeserializeAnimator(const json& j, cm::animation::AnimationPlayerComponent& a)
{
    const std::string mode = j.value("mode", "player");
    a.AnimatorMode = (mode == "controller") ? cm::animation::AnimationPlayerComponent::Mode::ControllerAnimated
                                              : cm::animation::AnimationPlayerComponent::Mode::AnimationPlayerAnimated;
    a.PlaybackSpeed = j.value("playbackSpeed", 1.0f);
    a.RootMotion = static_cast<cm::animation::AnimationPlayerComponent::RootMotionMode>(j.value("rootMotion", 0));
    a.ControllerPath = j.value("controllerPath", "");
    a.SingleClipPath = j.value("singleClipPath", "");
    a.PlayOnStart = j.value("playOnStart", true);
    if (a.ActiveStates.empty()) a.ActiveStates.push_back({});
    a.ActiveStates.front().Loop = j.value("loop", true);
    a.IsPlaying = false;
    a._InitApplied = false;
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

    // Animator
    if (entityData->AnimationPlayer) {
        data["animator"] = SerializeAnimator(*entityData->AnimationPlayer);
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
        entityData->Mesh = std::make_unique<MeshComponent>();
        DeserializeMesh(data["mesh"], *entityData->Mesh);
    }

    if (data.contains("light")) {
        entityData->Light = std::make_unique<LightComponent>();
        DeserializeLight(data["light"], *entityData->Light);
    }

    if (data.contains("collider")) {
        entityData->Collider = std::make_unique<ColliderComponent>();
        DeserializeCollider(data["collider"], *entityData->Collider);
    }

    if (data.contains("rigidbody")) {
        entityData->RigidBody = std::make_unique<RigidBodyComponent>();
        DeserializeRigidBody(data["rigidbody"], *entityData->RigidBody);
    }

    if (data.contains("staticbody")) {
        entityData->StaticBody = std::make_unique<StaticBodyComponent>();
        DeserializeStaticBody(data["staticbody"], *entityData->StaticBody);
    }

    if (data.contains("camera")) {
        entityData->Camera = std::make_unique<CameraComponent>();
        DeserializeCamera(data["camera"], *entityData->Camera);
    }
    if (data.contains("terrain")) {
        entityData->Terrain = std::make_unique<TerrainComponent>();
        DeserializeTerrain(data["terrain"], *entityData->Terrain);
    }
    if (data.contains("emitter")) {
        entityData->Emitter = std::make_unique<ParticleEmitterComponent>();
        DeserializeParticleEmitter(data["emitter"], *entityData->Emitter);
    }

    // UI Components
    if (data.contains("canvas")) {
        entityData->Canvas = std::make_unique<CanvasComponent>();
        DeserializeCanvas(data["canvas"], *entityData->Canvas);
    }
    if (data.contains("panel")) {
        entityData->Panel = std::make_unique<PanelComponent>();
        DeserializePanel(data["panel"], *entityData->Panel);
    }
    if (data.contains("button")) {
        entityData->Button = std::make_unique<ButtonComponent>();
        DeserializeButton(data["button"], *entityData->Button);
    }

    // Deserialize scripts
    if (data.contains("scripts")) {
        DeserializeScripts(data["scripts"], entityData->Scripts);
    }

    // Animator
    if (data.contains("animator")) {
        if (!entityData->AnimationPlayer) entityData->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>();
        DeserializeAnimator(data["animator"], *entityData->AnimationPlayer);
    }

    return id;
}

// Scene serialization
json Serializer::SerializeScene( Scene& scene) {
    json sceneData;
    sceneData["version"] = "1.0";
    sceneData["entities"] = json::array();

    // Build skip set for descendants of imported model roots and collect per-node overrides
    std::unordered_set<EntityID> skip;
    std::unordered_map<EntityID, nlohmann::json> rootOverrides;
    auto computeNodePath = [&](EntityID root, EntityID node) -> std::string {
        std::vector<std::string> parts; EntityID cur = node;
        while (cur != -1) {
            auto* d = scene.GetEntityData(cur); if (!d) break;
            if (cur == root) { parts.push_back(d->Name); break; }
            parts.push_back(d->Name); cur = d->Parent;
        }
        std::reverse(parts.begin(), parts.end());
        if (!parts.empty()) parts.erase(parts.begin()); // make path relative to model root
        std::string s; for (size_t i=0;i<parts.size();++i){ s += parts[i]; if (i+1<parts.size()) s += "/"; }
        return s;
    };
    for (const auto& e : scene.GetEntities()) {
        std::string path; ClaymoreGUID g{}; 
        if (IsImportedModelRoot(scene, e.GetID(), path, g)) {
            rootOverrides[e.GetID()] = nlohmann::json::array();
            // Walk descendants. Skip serializing them fully; store override blobs under the root instead
            std::function<void(EntityID)> walk = [&](EntityID id){
                auto* d = scene.GetEntityData(id); if (!d) return; 
                for (EntityID c : d->Children) {
                    skip.insert(c);
                    nlohmann::json childJ = SerializeEntity(c, scene);
                    childJ["_modelNodePath"] = computeNodePath(e.GetID(), c);
                    childJ.erase("id"); childJ.erase("parent"); childJ.erase("children"); childJ.erase("name"); childJ.erase("asset");
                    if (!childJ.empty()) rootOverrides[e.GetID()].push_back(std::move(childJ));
                    walk(c);
                }
            };
            walk(e.GetID());
            skip.erase(e.GetID());
        }
    }

    for (const auto& entity : scene.GetEntities()) {
        EntityID eid = entity.GetID();
        if (skip.find(eid) != skip.end()) continue;
        json entityData = SerializeEntity(eid, scene);
        // If this is an imported model root, attach compact asset record
        std::string path; ClaymoreGUID g{};
        if (IsImportedModelRoot(scene, eid, path, g)) {
            json asset;
            asset["type"] = "model";
            // save virtual path
            std::string v = path; for(char& c: v) if (c=='\\') c='/';
            auto pos = v.find("assets/"); if (pos != std::string::npos) v = v.substr(pos);
            asset["path"] = v;
            asset["guid"] = g.ToString();
            entityData["asset"] = std::move(asset);
            // attach collected per-node overrides (if any)
            auto it = rootOverrides.find(eid);
            entityData["children"] = (it != rootOverrides.end()) ? it->second : nlohmann::json::array();
        }
        if (!entityData.empty()) sceneData["entities"].push_back(entityData);
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
    // Keep track of roots that were instantiated from compact asset nodes (e.g., models).
    // Their internal hierarchy should remain intact; skip child clearing/parent fixup for them.
    std::unordered_set<EntityID> opaqueRoots;
    
    // Pre-scan: map oldId -> parentOld and set of all model-asset entity ids
    std::unordered_map<EntityID, EntityID> oldToParent;
    std::unordered_set<EntityID> modelAssetIds;
    for (const auto& ent : data["entities"]) {
        if (ent.contains("id") && ent.contains("parent")) {
            oldToParent[ ent["id"].get<EntityID>() ] = ent["parent"].get<EntityID>();
        }
        if (ent.contains("asset") && ent["asset"].is_object()) {
            const auto& a = ent["asset"]; if (a.value("type", "") == std::string("model") && ent.contains("id")) {
                modelAssetIds.insert(ent["id"].get<EntityID>());
            }
        }
    }

    for (const auto& entityData : data["entities"]) {
        // Preserve names exactly as authored. Create with base name without suffixing.
        EntityID newId = 0;
        if (entityData.contains("name")) {
            // Handle compact asset node: instantiate model instead of raw entity
            if (entityData.contains("asset") && entityData["asset"].is_object()) {
                const auto& a = entityData["asset"];
                std::string type = a.value("type", "");
                if (type == "model") {
                    // Skip nested model-asset nodes to avoid duplicate instantiation
                    if (entityData.contains("id") && entityData.contains("parent")) {
                        EntityID curParent = entityData["parent"].get<EntityID>();
                        while (curParent != (EntityID)0 && curParent != (EntityID)-1) {
                            if (modelAssetIds.count(curParent)) {
                                // Parent (or ancestor) is a model asset root; skip this nested model node
                                if (entityData.contains("id")) {
                                    idMapping[entityData["id"].get<EntityID>()] = 0; // map to 0 to keep relations safe
                                }
                                goto NEXT_ENTITY; // continue outer loop
                            }
                            auto itp = oldToParent.find(curParent);
                            if (itp == oldToParent.end()) break;
                            curParent = itp->second;
                        }
                    }
                    std::string p = a.value("path", "");
                    // Use project-root relative virtual path; prefer cached .meta fast path if present
                    std::string resolved = p;
                    if (!resolved.empty() && !fs::exists(resolved)) {
                        resolved = (Project::GetProjectDirectory() / p).string();
                    }
                    // Normalize slashes
                    for (char& c : resolved) if (c=='\\') c = '/';
                    // Determine spawn position
                    glm::vec3 pos(0.0f);
                    if (entityData.contains("transform")) {
                        auto t = entityData["transform"];
                        if (t.contains("position")) pos = DeserializeVec3(t["position"]);
                    }
                    // Prefer sibling .meta (fast path)
                    std::string metaTry = resolved;
                    std::string ext = fs::path(resolved).extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext != ".meta") {
                        fs::path rp(resolved);
                        fs::path metaPath = rp.parent_path() / (rp.stem().string() + ".meta");
                        if (fs::exists(metaPath)) metaTry = metaPath.string();
                    }
                    if (!metaTry.empty() && fs::path(metaTry).extension() == ".meta") {
                        newId = scene.InstantiateModelFast(metaTry, pos);
                        if (newId == (EntityID)0 || newId == (EntityID)-1) {
                            // Fallback to slow path if fast path failed
                            newId = scene.InstantiateModel(resolved, pos);
                        }
                    } else {
                        newId = scene.InstantiateModel(resolved, pos);
                    }
                    if (newId != 0) {
                        opaqueRoots.insert(newId);
                        // Apply transform fully to the root entity
                        if (auto* ed = scene.GetEntityData(newId)) {
                            if (entityData.contains("transform")) DeserializeTransform(entityData["transform"], ed->Transform);
                            // Apply scripts on root if any
                            if (entityData.contains("scripts")) DeserializeScripts(entityData["scripts"], ed->Scripts);
                            if (entityData.contains("animator")) {
                                if (!ed->AnimationPlayer) ed->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>();
                                DeserializeAnimator(entityData["animator"], *ed->AnimationPlayer);
                            }
                            // Post-instantiate: if skeleton exists but BoneEntities unresolved, rebuild by name/path
                            std::function<SkeletonComponent*(EntityID, EntityID&)> findSkel = [&](EntityID id, EntityID& out)->SkeletonComponent*{
                                if (auto* d = scene.GetEntityData(id)) {
                                    if (d->Skeleton) { out = id; return d->Skeleton.get(); }
                                    for (EntityID c : d->Children) { if (auto* s = findSkel(c, out)) return s; }
                                }
                                return nullptr;
                            };
                            EntityID skelEntity = (EntityID)-1; if (auto* sk = findSkel(newId, skelEntity)) {
                                bool needsRebind = sk->BoneEntities.size() != sk->InverseBindPoses.size();
                                if (!needsRebind) {
                                    for (auto id : sk->BoneEntities) { if (id == (EntityID)-1) { needsRebind = true; break; } }
                                }
                                if (needsRebind) {
                                    std::unordered_map<std::string, EntityID> pathMap;
                                    std::function<void(EntityID, const std::string&)> dfs = [&](EntityID id, const std::string& path){
                                        if (auto* d = scene.GetEntityData(id)) {
                                            pathMap[path] = id;
                                            for (EntityID c : d->Children) if (auto* cd = scene.GetEntityData(c)) dfs(c, path.empty() ? cd->Name : (path + "/" + cd->Name));
                                        }
                                    };
                                    if (auto* rd = scene.GetEntityData(newId)) dfs(newId, rd->Name);
                                    const size_t n = sk->InverseBindPoses.size();
                                    sk->BoneEntities.assign(n, (EntityID)-1);
                                    // Build index->name list
                                    std::vector<std::string> boneNames(n, std::string());
                                    for (const auto& kv : sk->BoneNameToIndex) { int idx = kv.second; if (idx >= 0 && (size_t)idx < n) boneNames[(size_t)idx] = kv.first; }
                                    for (size_t i = 0; i < n; ++i) {
                                        const std::string& bname = boneNames[i];
                                        if (bname.empty()) continue;
                                        for (const auto& kv : pathMap) {
                                            const std::string& full = kv.first; size_t s = full.find_last_of('/');
                                            std::string last = (s == std::string::npos) ? full : full.substr(s+1);
                                            if (last == bname) { sk->BoneEntities[i] = kv.second; break; }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (newId != 0 && entityData.contains("id")) {
                        idMapping[entityData["id"].get<EntityID>()] = newId;
                    }
                    continue; // handled compact node
                }
            }
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
            if (copy.contains("mesh")) { ed->Mesh = std::make_unique<MeshComponent>(); DeserializeMesh(copy["mesh"], *ed->Mesh); }
            if (copy.contains("light")) { ed->Light = std::make_unique<LightComponent>(); DeserializeLight(copy["light"], *ed->Light); }
            if (copy.contains("collider")) { ed->Collider = std::make_unique<ColliderComponent>(); DeserializeCollider(copy["collider"], *ed->Collider); }
            if (copy.contains("rigidbody")) { ed->RigidBody = std::make_unique<RigidBodyComponent>(); DeserializeRigidBody(copy["rigidbody"], *ed->RigidBody); }
            if (copy.contains("staticbody")) { ed->StaticBody = std::make_unique<StaticBodyComponent>(); DeserializeStaticBody(copy["staticbody"], *ed->StaticBody); }
            if (copy.contains("camera")) { ed->Camera = std::make_unique<CameraComponent>(); DeserializeCamera(copy["camera"], *ed->Camera); }
            if (copy.contains("terrain")) { ed->Terrain = std::make_unique<TerrainComponent>(); DeserializeTerrain(copy["terrain"], *ed->Terrain); }
            if (copy.contains("emitter")) { ed->Emitter = std::make_unique<ParticleEmitterComponent>(); DeserializeParticleEmitter(copy["emitter"], *ed->Emitter); }
            if (copy.contains("canvas")) { ed->Canvas = std::make_unique<CanvasComponent>(); DeserializeCanvas(copy["canvas"], *ed->Canvas); }
            if (copy.contains("panel")) { ed->Panel = std::make_unique<PanelComponent>(); DeserializePanel(copy["panel"], *ed->Panel); }
            if (copy.contains("button")) { ed->Button = std::make_unique<ButtonComponent>(); DeserializeButton(copy["button"], *ed->Button); }
            if (copy.contains("scripts")) { DeserializeScripts(copy["scripts"], ed->Scripts); }
        } else {
            newId = DeserializeEntity(entityData, scene);
        }
        if (newId != 0 && entityData.contains("id")) {
            EntityID oldId = entityData["id"];
            idMapping[oldId] = newId;
        }
        NEXT_ENTITY: ;
    }

    // Reset children vectors to avoid duplicates for non-opaque roots, then fix up parent-child relationships
    for (const auto& [oldId, newId] : idMapping) {
        if (opaqueRoots.find(newId) != opaqueRoots.end()) continue;
        if (auto* ed = scene.GetEntityData(newId)) ed->Children.clear();
    }
    // Second pass: Fix up parent-child relationships (skip opaque roots that already have a hierarchy)
    for (const auto& entityData : data["entities"]) {
        if (entityData.contains("id") && entityData.contains("parent")) {
            EntityID oldId = entityData["id"];
            EntityID oldParent = entityData["parent"];
            
            if (idMapping.find(oldId) != idMapping.end() && 
                idMapping.find(oldParent) != idMapping.end()) {
                EntityID childNew = idMapping[oldId];
                EntityID parentNew = idMapping[oldParent];
                if (opaqueRoots.find(childNew) != opaqueRoots.end()) continue;
                if (opaqueRoots.find(parentNew) != opaqueRoots.end()) continue;
                scene.SetParent(childNew, parentNew);
            }
        }
    }

    // Apply per-node overrides under compact model roots
    for (const auto& entityData : data["entities"]) {
        if (!(entityData.contains("asset") && entityData["asset"].is_object())) continue;
        const auto& a = entityData["asset"]; if (a.value("type", "") != std::string("model")) continue;
        if (!entityData.contains("id")) continue;
        auto itMap = idMapping.find(entityData["id"].get<EntityID>()); if (itMap == idMapping.end()) continue;
        EntityID rootNew = itMap->second;
        if (!entityData.contains("children") || !entityData["children"].is_array()) continue;
        for (const auto& childOverride : entityData["children"]) {
            if (!childOverride.contains("_modelNodePath")) continue;
            std::string relPath = childOverride["_modelNodePath"].get<std::string>();
            // Resolve target entity by walking names from root
            EntityID target = rootNew;
            if (!relPath.empty()) {
                std::stringstream ss(relPath); std::string part;
                while (std::getline(ss, part, '/')) {
                    auto* d = scene.GetEntityData(target); if (!d) break;
                    EntityID next = -1; for (EntityID c : d->Children) { auto* cd = scene.GetEntityData(c); if (cd && cd->Name == part) { next = c; break; } }
                    if (next == -1) { target = -1; break; } target = next;
                }
            }
            if (target == -1) continue;
            auto* td = scene.GetEntityData(target); if (!td) continue;
            if (childOverride.contains("transform")) { DeserializeTransform(childOverride["transform"], td->Transform); td->Transform.TransformDirty = true; }
            if (childOverride.contains("mesh") && td->Mesh) { DeserializeMesh(childOverride["mesh"], *td->Mesh); }
            if (childOverride.contains("light") && td->Light) { DeserializeLight(childOverride["light"], *td->Light); }
            if (childOverride.contains("collider") && td->Collider) { DeserializeCollider(childOverride["collider"], *td->Collider); }
            if (childOverride.contains("rigidbody") && td->RigidBody) { DeserializeRigidBody(childOverride["rigidbody"], *td->RigidBody); }
            if (childOverride.contains("staticbody") && td->StaticBody) { DeserializeStaticBody(childOverride["staticbody"], *td->StaticBody); }
            if (childOverride.contains("camera") && td->Camera) { DeserializeCamera(childOverride["camera"], *td->Camera); }
            if (childOverride.contains("terrain") && td->Terrain) { DeserializeTerrain(childOverride["terrain"], *td->Terrain); }
            if (childOverride.contains("emitter") && td->Emitter) { DeserializeParticleEmitter(childOverride["emitter"], *td->Emitter); }
            if (childOverride.contains("canvas") && td->Canvas) { DeserializeCanvas(childOverride["canvas"], *td->Canvas); }
            if (childOverride.contains("panel") && td->Panel) { DeserializePanel(childOverride["panel"], *td->Panel); }
            if (childOverride.contains("button") && td->Button) { DeserializeButton(childOverride["button"], *td->Button); }
            if (childOverride.contains("scripts")) { DeserializeScripts(childOverride["scripts"], td->Scripts); }
            if (childOverride.contains("animator") && td->AnimationPlayer) { DeserializeAnimator(childOverride["animator"], *td->AnimationPlayer); }
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
        json sceneData;
        // Try virtual filesystem first
        {
            std::string sceneText;
            if (FileSystem::Instance().ReadTextFile(filepath, sceneText)) {
                sceneData = json::parse(sceneText);
            } else {
                if (!fs::exists(filepath)) {
                    std::cerr << "[Serializer] Scene file does not exist: " << filepath << std::endl;
                    return false;
                }
                std::ifstream file(filepath);
                if (!file.is_open()) {
                    std::cerr << "[Serializer] Failed to open scene file: " << filepath << std::endl;
                    return false;
                }
                file >> sceneData;
                file.close();
            }
        }
        
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
    if (entityData.AnimationPlayer) {
        entityJson["animator"] = SerializeAnimator(*entityData.AnimationPlayer);
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
        entityData.Mesh = std::make_unique<MeshComponent>();
        DeserializeMesh(entityJson["mesh"], *entityData.Mesh);
    }

    if (entityJson.contains("light")) {
        entityData.Light = std::make_unique<LightComponent>();
        DeserializeLight(entityJson["light"], *entityData.Light);
    }

    if (entityJson.contains("collider")) {
        entityData.Collider = std::make_unique<ColliderComponent>();
        DeserializeCollider(entityJson["collider"], *entityData.Collider);
    }

    // Deserialize scripts
    if (entityJson.contains("scripts")) {
        DeserializeScripts(entityJson["scripts"], entityData.Scripts);
    }
    if (entityJson.contains("animator")) {
        entityData.AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>();
        DeserializeAnimator(entityJson["animator"], *entityData.AnimationPlayer);
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