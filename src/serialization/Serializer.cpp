#include "Serializer.h"
#include <fstream>
#include <iostream>
#include "rendering/ModelBuild.h"
#include <sstream>
#include <filesystem>
#include <algorithm>
#include "ecs/AnimationComponents.h"
#include "scripting/ScriptSystem.h"
#include "ecs/EntityData.h"
#include "pipeline/AssetLibrary.h"
#include "rendering/TextureLoader.h"
#include "ecs/UIComponents.h"
#include "animation/AnimationPlayerComponent.h"
#include "io/FileSystem.h"
#include <editor/Project.h>
#include <unordered_set>

#include "ecs/Scene.h"

// Jobs for parallel scene deserialization
#include "jobs/Jobs.h"
#include "jobs/ParallelFor.h"

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
    // Preserve quaternion-based rotation when authoring uses it
    data["useQuatRotation"] = transform.UseQuatRotation;
    data["rotationQ"] = json::array({ transform.RotationQ.w, transform.RotationQ.x, transform.RotationQ.y, transform.RotationQ.z });
    data["localMatrix"] = SerializeMat4(transform.LocalMatrix);
    data["worldMatrix"] = SerializeMat4(transform.WorldMatrix);
    data["transformDirty"] = transform.TransformDirty;
    return data;
}

void Serializer::DeserializeTransform(const json& data, TransformComponent& transform) {
    if (data.contains("position")) transform.Position = DeserializeVec3(data["position"]);
    if (data.contains("rotation")) transform.Rotation = DeserializeVec3(data["rotation"]);
    if (data.contains("scale")) transform.Scale = DeserializeVec3(data["scale"]);
    // Quaternions (preferred if present)
    if (data.contains("rotationQ") && data["rotationQ"].is_array() && data["rotationQ"].size() == 4) {
        // Stored as [w,x,y,z] for readability
        transform.RotationQ = glm::quat(
            (float)data["rotationQ"][0],
            (float)data["rotationQ"][1],
            (float)data["rotationQ"][2],
            (float)data["rotationQ"][3]
        );
        transform.UseQuatRotation = true;
    }
    if (data.contains("useQuatRotation")) {
        bool uqr = data["useQuatRotation"].get<bool>();
        // If rotationQ missing but flag true, derive from Euler now
        if (uqr && !(data.contains("rotationQ") && data["rotationQ"].is_array())) {
            glm::mat4 r = glm::yawPitchRoll(
                glm::radians(transform.Rotation.y),
                glm::radians(transform.Rotation.x),
                glm::radians(transform.Rotation.z));
            transform.RotationQ = glm::quat_cast(r);
        }
        transform.UseQuatRotation = uqr;
    }
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
                case 3: mesh.mesh = StandardMeshManager::Instance().GetCapsuleMesh(); break;
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
            } else if (mesh.MeshName == "Capsule") {
                mesh.mesh = StandardMeshManager::Instance().GetCapsuleMesh();
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

// ---------------- Skeleton & Skinning ----------------
json Serializer::SerializeSkeleton(const SkeletonComponent& skeleton) {
    json j;
    // Matrices
    j["inverseBindPoses"] = json::array();
    for (const auto& m : skeleton.InverseBindPoses) j["inverseBindPoses"].push_back(SerializeMat4(m));
    j["bindPoseGlobals"] = json::array();
    for (const auto& m : skeleton.BindPoseGlobals) j["bindPoseGlobals"].push_back(SerializeMat4(m));

    // Bone parents
    if (!skeleton.BoneParents.empty()) j["boneParents"] = skeleton.BoneParents;

    // Bone names (index -> name)
    if (!skeleton.BoneNameToIndex.empty()) {
        // Emit as array aligned with indices for stability
        size_t count = 0;
        for (const auto& kv : skeleton.BoneNameToIndex) count = std::max(count, (size_t)std::max(0, kv.second+1));
        json names = json::array();
        for (size_t i = 0; i < count; ++i) names.push_back(nullptr);
        for (const auto& kv : skeleton.BoneNameToIndex) {
            int idx = kv.second; if (idx < 0) continue;
            while ((size_t)idx >= names.size()) names.push_back(nullptr);
            names[(size_t)idx] = kv.first;
        }
        j["boneNames"] = std::move(names);
    }
    // Stable GUIDs
    if (skeleton.SkeletonGuid.high != 0 || skeleton.SkeletonGuid.low != 0) j["skeletonGuid"] = skeleton.SkeletonGuid;
    if (!skeleton.JointGuids.empty()) {
        j["jointGuids"] = json::array();
        for (uint64_t g : skeleton.JointGuids) j["jointGuids"].push_back(g);
    }
    return j;
}

void Serializer::DeserializeSkeleton(const json& j, SkeletonComponent& skeleton) {
    skeleton.InverseBindPoses.clear(); skeleton.BindPoseGlobals.clear();
    skeleton.BoneParents.clear(); skeleton.BoneNameToIndex.clear();
    skeleton.BoneNames.clear(); skeleton.JointGuids.clear(); skeleton.SkeletonGuid = ClaymoreGUID{};

    if (j.contains("inverseBindPoses") && j["inverseBindPoses"].is_array()) {
        for (const auto& m : j["inverseBindPoses"]) skeleton.InverseBindPoses.push_back(DeserializeMat4(m));
    }
    if (j.contains("bindPoseGlobals") && j["bindPoseGlobals"].is_array()) {
        for (const auto& m : j["bindPoseGlobals"]) skeleton.BindPoseGlobals.push_back(DeserializeMat4(m));
    }
    if (j.contains("boneParents") && j["boneParents"].is_array()) {
        skeleton.BoneParents.clear();
        for (const auto& v : j["boneParents"]) skeleton.BoneParents.push_back(v.get<int>());
    }
    if (j.contains("boneNames") && j["boneNames"].is_array()) {
        const auto& arr = j["boneNames"];
        for (size_t i = 0; i < arr.size(); ++i) {
            if (!arr[i].is_null()) skeleton.BoneNameToIndex[arr[i].get<std::string>()] = (int)i;
        }
        // Also store aligned bone names for convenience
        skeleton.BoneNames.resize(arr.size());
        for (size_t i = 0; i < arr.size(); ++i) {
            if (!arr[i].is_null()) skeleton.BoneNames[i] = arr[i].get<std::string>();
        }
    }
    // BoneEntities are scene-local; don't persist raw ids. Rebind later using names.
    skeleton.BoneEntities.assign(skeleton.InverseBindPoses.size(), (EntityID)-1);

    // Stable GUIDs
    try { if (j.contains("skeletonGuid")) j.at("skeletonGuid").get_to(skeleton.SkeletonGuid); } catch(...) {}
    if (j.contains("jointGuids") && j["jointGuids"].is_array()) {
        const auto& arr = j["jointGuids"];
        skeleton.JointGuids.resize(arr.size());
        for (size_t i = 0; i < arr.size(); ++i) skeleton.JointGuids[i] = arr[i].get<uint64_t>();
    }
}

json Serializer::SerializeSkinning(const SkinningComponent& skinning) {
    json j;
    // Do not serialize palette (runtime). Persist link to skeleton by name for robustness.
    j["skeletonRoot"] = skinning.SkeletonRoot; // temporary; may be -1. A post-load fixup will correct if needed.
    return j;
}

void Serializer::DeserializeSkinning(const json& j, SkinningComponent& skinning) {
    skinning.Palette.clear();
    skinning.SkeletonRoot = j.value("skeletonRoot", (EntityID)-1);
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
    data["scale"] = { panel.Scale.x, panel.Scale.y };
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
    data["fillMode"] = (int)panel.Mode;
    data["tileRepeat"] = { panel.TileRepeat.x, panel.TileRepeat.y };
    data["sliceUV"] = { panel.SliceUV.x, panel.SliceUV.y, panel.SliceUV.z, panel.SliceUV.w };
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
    if (data.contains("scale") && data["scale"].is_array() && data["scale"].size() == 2) {
        panel.Scale.x = data["scale"][0];
        panel.Scale.y = data["scale"][1];
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
    if (data.contains("fillMode")) panel.Mode = (PanelComponent::FillMode)((int)data["fillMode"]);
    if (data.contains("tileRepeat") && data["tileRepeat"].is_array() && data["tileRepeat"].size() == 2) {
        panel.TileRepeat.x = data["tileRepeat"][0];
        panel.TileRepeat.y = data["tileRepeat"][1];
    }
    if (data.contains("sliceUV") && data["sliceUV"].is_array() && data["sliceUV"].size() == 4) {
        panel.SliceUV.x = data["sliceUV"][0];
        panel.SliceUV.y = data["sliceUV"][1];
        panel.SliceUV.z = data["sliceUV"][2];
        panel.SliceUV.w = data["sliceUV"][3];
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
   // Stable GUID and optional prefab source vpath
   data["guid"] = entityData->EntityGuid;
   if (!entityData->PrefabSource.empty()) {
       std::string v = FileSystem::Normalize(entityData->PrefabSource);
       auto pos = v.find("assets/");
       if (pos != std::string::npos) v = v.substr(pos);
       data["prefabSource"] = v;
   }

   // Serialize components
   data["transform"] = SerializeTransform(entityData->Transform);

   if (entityData->Mesh) {
      data["mesh"] = SerializeMesh(*entityData->Mesh);
      }

   if (entityData->Light) {
      data["light"] = SerializeLight(*entityData->Light);
      }

   // Skeleton & Skinning
   if (entityData->Skeleton) {
       data["skeleton"] = SerializeSkeleton(*entityData->Skeleton);
   }
   if (entityData->Skinning) {
       data["skinning"] = SerializeSkinning(*entityData->Skinning);
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

   // Merge unknown/extra fields to preserve forward-compatibility
   if (entityData->Extra.is_object()) {
       for (auto it = entityData->Extra.begin(); it != entityData->Extra.end(); ++it) {
           if (!data.contains(it.key())) data[it.key()] = it.value();
       }
   }
   return data;
   }


EntityID Serializer::DeserializeEntity(const json& data, Scene& scene) {
    if (!data.contains("name")) return 0;

    std::string name = data["name"];
    // Use exact-name creation during deserialization to avoid suffix-based clones
    Entity entity = scene.CreateEntityExact(name);
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
    // GUID & prefab source
    if (data.contains("guid")) {
        try { data.at("guid").get_to(entityData->EntityGuid); } catch(...) {}
    } else {
        entityData->EntityGuid = ClaymoreGUID::Generate();
    }
    if (data.contains("prefabSource")) {
        try { entityData->PrefabSource = FileSystem::Normalize(data.at("prefabSource").get<std::string>()); } catch(...) {}
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
    // Animation-related
    if (data.contains("skeleton")) {
        entityData->Skeleton = std::make_unique<SkeletonComponent>();
        DeserializeSkeleton(data["skeleton"], *entityData->Skeleton);
    }
    if (data.contains("skinning")) {
        entityData->Skinning = std::make_unique<SkinningComponent>();
        DeserializeSkinning(data["skinning"], *entityData->Skinning);
    }
    // Ensure skinned material for skinned meshes
    if (entityData->Skinning && entityData->Mesh) {
        if (!std::dynamic_pointer_cast<SkinnedPBRMaterial>(entityData->Mesh->material)) {
            entityData->Mesh->material = MaterialManager::Instance().CreateSkinnedPBRMaterial();
        }
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
    // Preserve unknown fields not recognized by this serializer
    try {
        static const std::unordered_set<std::string> kKnown = {
            "id","name","layer","tag","parent","children","guid","prefabSource",
            "transform","mesh","light","collider","rigidbody","staticbody","camera",
            "terrain","emitter","canvas","panel","button","scripts","animator","asset",
            "skeleton","skinning"
        };
        entityData->Extra = nlohmann::json::object();
        for (auto it = data.begin(); it != data.end(); ++it) {
            if (kKnown.find(it.key()) == kKnown.end()) {
                entityData->Extra[it.key()] = it.value();
            }
        }
    } catch(...) {}

    return id;
}

// Scene serialization
json Serializer::SerializeScene( Scene& scene) {
    json sceneData;
    sceneData["version"] = "1.0";
    sceneData["entities"] = json::array();
    // Environment
    try {
        const Environment& env = scene.GetEnvironment();
        json jenv;
        jenv["ambientMode"] = (env.Ambient == Environment::AmbientMode::FlatColor) ? "FlatColor" : "Skybox";
        jenv["ambientColor"] = SerializeVec3(env.AmbientColor);
        jenv["ambientIntensity"] = env.AmbientIntensity;
        jenv["useSkybox"] = env.UseSkybox;
        // Skybox texture path not serialized yet (TextureCube asset system pending)
        jenv["exposure"] = env.Exposure;
        jenv["fogEnabled"] = env.EnableFog;
        jenv["fogColor"] = SerializeVec3(env.FogColor);
        jenv["fogDensity"] = env.FogDensity;
        jenv["proceduralSky"] = env.ProceduralSky;
        jenv["skyZenithColor"] = SerializeVec3(env.SkyZenithColor);
        jenv["skyHorizonColor"] = SerializeVec3(env.SkyHorizonColor);
        sceneData["environment"] = std::move(jenv);
    } catch(...) {}
    // Optional: include an asset map to help resolve GUIDs across different working copies
    // We serialize all known AssetLibrary mappings as a portable hint.
    try {
        auto all = AssetLibrary::Instance().GetAllAssets();
        if (!all.empty()) {
            json amap = json::array();
            for (const auto& rec : all) {
                const std::string& path = std::get<0>(rec);
                const ClaymoreGUID& guid = std::get<1>(rec);
                if (path.empty()) continue;
                json j;
                j["guid"] = guid.ToString();
                j["path"] = path;
                amap.push_back(std::move(j));
            }
            if (!amap.empty()) sceneData["assetMap"] = std::move(amap);
        }
    } catch(...) {}

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
                    // Keep name to persist renames; strip relational/id-only fields
                    childJ.erase("id"); childJ.erase("parent"); childJ.erase("children"); childJ.erase("asset");
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

    try {
        std::cout << "[DeserializeBegin] version=" << data.value("version", "")
                  << " entities=" << (data.contains("entities") && data["entities"].is_array() ? data["entities"].size() : 0)
                  << std::endl;
    } catch(...) {}

    // If the scene carries an assetMap, pre-register GUID→path so asset references resolve
    try {
        if (data.contains("assetMap") && data["assetMap"].is_array()) {
            for (const auto& rec : data["assetMap"]) {
                std::string gstr = rec.value("guid", "");
                std::string vpath = rec.value("path", "");
                if (gstr.empty() || vpath.empty()) continue;
                ClaymoreGUID g = ClaymoreGUID::FromString(gstr);
                // Register with generic Mesh type; actual type is not required for path resolution
                AssetLibrary::Instance().RegisterAsset(AssetReference(g, 0, (int)AssetType::Mesh), AssetType::Mesh, vpath, vpath);
            }
        }
    } catch(...) {}

    // Telemetry: count components and unknown blocks before mutating scene
    try {
        const auto& ents = data["entities"];
        size_t numEntities = ents.is_array() ? ents.size() : 0;
        size_t componentCount = 0;
        size_t unknownBlocks = 0;
        std::unordered_set<std::string> known = {
            "id","name","layer","tag","parent","children","guid","prefabSource",
            "transform","mesh","light","collider","rigidbody","staticbody","camera",
            "terrain","emitter","canvas","panel","button","scripts","animator","asset",
            "skeleton","skinning"
        };
        std::unordered_set<std::string> guidSeen;
        size_t guidMissing = 0, guidDup = 0;
        for (const auto& e : ents) {
            if (!e.is_object()) continue;
            for (auto it = e.begin(); it != e.end(); ++it) {
                const std::string& k = it.key();
                if (k == "transform" || k == "mesh" || k == "light" || k == "collider" || k == "rigidbody" || k == "staticbody" || k == "camera" || k == "terrain" || k == "emitter" || k == "canvas" || k == "panel" || k == "button" || k == "scripts" || k == "animator")
                    componentCount++;
                if (known.find(k) == known.end()) unknownBlocks++;
            }
            if (e.contains("guid")) {
                try {
                    ClaymoreGUID g; e.at("guid").get_to(g);
                    std::string gs = g.ToString();
                    if (!guidSeen.insert(gs).second) guidDup++;
                } catch(...) {}
            } else {
                guidMissing++;
            }
        }
        std::cout << "[Deserialize] version=" << data.value("version", "")
                  << " entities=" << numEntities
                  << " components=" << componentCount
                  << " unknown_blocks=" << unknownBlocks
                  << " guid_missing=" << guidMissing
                  << " guid_dupes=" << guidDup << std::endl;
    } catch(...) {}

    // Apply environment if present
    if (data.contains("environment") && data["environment"].is_object()) {
        try {
            Environment& env = scene.GetEnvironment();
            const json& jenv = data["environment"];
            std::string mode = jenv.value("ambientMode", "FlatColor");
            env.Ambient = (mode == "Skybox") ? Environment::AmbientMode::Skybox : Environment::AmbientMode::FlatColor;
            if (jenv.contains("ambientColor")) env.AmbientColor = DeserializeVec3(jenv["ambientColor"]);
            env.AmbientIntensity = jenv.value("ambientIntensity", env.AmbientIntensity);
            env.UseSkybox = jenv.value("useSkybox", env.UseSkybox);
            env.Exposure = jenv.value("exposure", env.Exposure);
            env.EnableFog = jenv.value("fogEnabled", env.EnableFog);
            if (jenv.contains("fogColor")) env.FogColor = DeserializeVec3(jenv["fogColor"]);
            env.FogDensity = jenv.value("fogDensity", env.FogDensity);
            env.ProceduralSky = jenv.value("proceduralSky", env.ProceduralSky);
            if (jenv.contains("skyZenithColor")) env.SkyZenithColor = DeserializeVec3(jenv["skyZenithColor"]);
            if (jenv.contains("skyHorizonColor")) env.SkyHorizonColor = DeserializeVec3(jenv["skyHorizonColor"]);
        } catch(...) {}
    }

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

    auto isDescendantOfModelAsset = [&](EntityID oldId) -> bool {
        if (modelAssetIds.empty()) return false;
        EntityID cur = oldId;
        size_t guard = 0;
        while (cur != (EntityID)0 && cur != (EntityID)-1 && guard++ < 100000) {
            auto itp = oldToParent.find(cur);
            if (itp == oldToParent.end()) break;
            EntityID p = itp->second;
            if (modelAssetIds.count(p)) return true;
            cur = p;
        }
        return false;
    };

    auto looksModelNode = [&](const json& j) -> bool {
        auto has = [&](const char* k){ return j.contains(k); };
        bool hasMesh = has("mesh");
        bool hasUserComp = has("camera") || has("light") || has("collider") || has("rigidbody") || has("staticbody") ||
                           has("emitter") || has("canvas") || has("panel") || has("button") || has("scripts") || has("terrain") ||
                           (has("animator") && !j["animator"].is_null());
        return hasMesh && !hasUserComp;
    };

    for (const auto& entityData : data["entities"]) {
        // Preserve names exactly as authored. Create with base name without suffixing.
        EntityID newId = 0;
        if (entityData.contains("name")) {
            // If this entry is a descendant of a model asset root and looks like an original model node,
            // skip creating it now to avoid duplicates. The importer will create the canonical node.
            if (entityData.contains("id")) {
                EntityID oldId = entityData["id"].get<EntityID>();
                if (!entityData.contains("asset") && isDescendantOfModelAsset(oldId) && looksModelNode(entityData)) {
                    std::cout << "[Skip] Model descendant original node id=" << oldId << " name=" << entityData["name"].get<std::string>() << std::endl;
                    // Intentionally do not map this oldId so parenting that targets it will be detected as unresolved
                    goto NEXT_ENTITY;
                }
            }
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
                                // Parent (or ancestor) is a model asset root; skip this nested model node entirely
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
                    // Register this model asset mapping so subsequent serialization/deserialization can resolve by GUID
                    try {
                        std::string gstr = a.value("guid", "");
                        if (!gstr.empty()) {
                            ClaymoreGUID g = ClaymoreGUID::FromString(gstr);
                            if (!(g.high == 0 && g.low == 0)) {
                                std::string v = p; for (char& ch : v) if (ch=='\\') ch = '/';
                                AssetLibrary::Instance().RegisterAsset(AssetReference(g, 0, (int)AssetType::Mesh), AssetType::Mesh, v, v);
                                if (!resolved.empty()) AssetLibrary::Instance().RegisterPathAlias(g, resolved);
                            }
                        }
                    } catch(...) {}
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
                            if (entityData.contains("name")) { ed->Name = entityData["name"].get<std::string>(); }
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
                                    for (const auto& id : sk->BoneEntities) { if (id == (EntityID)-1) { needsRebind = true; break; } }
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
            Entity temp = scene.CreateEntityExact(entityData["name"]);
            newId = temp.GetID();
            auto* ed = scene.GetEntityData(newId);
            if (ed) ed->Name = entityData["name"]; // avoid auto-suffix pattern
            try {
                std::cout << "[Create] guid=" << ed->EntityGuid.ToString() << " name=" << ed->Name << " src=Deserialize" << std::endl;
            } catch(...) {}
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
            // GUID & prefab source
            if (copy.contains("guid")) {
                try { copy.at("guid").get_to(ed->EntityGuid); } catch(...) {}
            } else {
                ed->EntityGuid = ClaymoreGUID::Generate();
            }
            if (copy.contains("prefabSource")) {
                try { ed->PrefabSource = FileSystem::Normalize(copy.at("prefabSource").get<std::string>()); } catch(...) {}
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
            // Preserve unknown fields
            try {
                static const std::unordered_set<std::string> kKnown = {
                    "id","name","layer","tag","parent","children","guid","prefabSource",
                    "transform","mesh","light","collider","rigidbody","staticbody","camera",
                    "terrain","emitter","canvas","panel","button","scripts","animator","asset",
                    "skeleton","skinning"
                };
                ed->Extra = nlohmann::json::object();
                for (auto it = entityData.begin(); it != entityData.end(); ++it) {
                    if (kKnown.find(it.key()) == kKnown.end()) {
                        ed->Extra[it.key()] = it.value();
                    }
                }
            } catch(...) {}
        } else {
            newId = DeserializeEntity(entityData, scene);
        }
        if (newId != 0 && entityData.contains("id")) {
            EntityID oldId = entityData["id"];
            idMapping[oldId] = newId;
        }
        NEXT_ENTITY: ;
    }

    // Parallelize component population for non-opaque roots (safe, no bgfx calls here)
    if (!idMapping.empty()) {
        std::vector<const json*> work;
        work.reserve(idMapping.size());
        for (const auto& entityData : data["entities"]) {
            if (!entityData.contains("id")) continue;
            EntityID oldId = entityData["id"].get<EntityID>();
            auto it = idMapping.find(oldId);
            if (it == idMapping.end()) continue;
            EntityID nid = it->second;
            if (opaqueRoots.find(nid) != opaqueRoots.end()) continue; // skip compact model asset roots
            // Also skip if this JSON node was a compact model node (handled already)
            if (entityData.contains("asset") && entityData["asset"].is_object()) continue;
            work.push_back(&entityData);
        }
        if (!work.empty()) {
            auto& js = Jobs();
            const size_t chunk = 32;
            parallel_for(js, size_t(0), work.size(), chunk, [&](size_t s, size_t c){
                for (size_t off = 0; off < c; ++off) {
                    const json& entityData = *work[s + off];
                    EntityID oldId = entityData.value("id", (EntityID)0);
                    auto it = idMapping.find(oldId); if (it == idMapping.end()) continue;
                    EntityID nid = it->second;
                    auto* ed = scene.GetEntityData(nid); if (!ed) continue;
                    // Transform
                    if (entityData.contains("transform")) { DeserializeTransform(entityData["transform"], ed->Transform); }
                    // Component shells + JSON decode (no GPU work)
                    if (entityData.contains("mesh")) { if (!ed->Mesh) ed->Mesh = std::make_unique<MeshComponent>(); DeserializeMesh(entityData["mesh"], *ed->Mesh); }
                    if (entityData.contains("light")) { if (!ed->Light) ed->Light = std::make_unique<LightComponent>(); DeserializeLight(entityData["light"], *ed->Light); }
                    if (entityData.contains("collider")) { if (!ed->Collider) ed->Collider = std::make_unique<ColliderComponent>(); DeserializeCollider(entityData["collider"], *ed->Collider); }
                    if (entityData.contains("rigidbody")) { if (!ed->RigidBody) ed->RigidBody = std::make_unique<RigidBodyComponent>(); DeserializeRigidBody(entityData["rigidbody"], *ed->RigidBody); }
                    if (entityData.contains("staticbody")) { if (!ed->StaticBody) ed->StaticBody = std::make_unique<StaticBodyComponent>(); DeserializeStaticBody(entityData["staticbody"], *ed->StaticBody); }
                    if (entityData.contains("camera")) { if (!ed->Camera) ed->Camera = std::make_unique<CameraComponent>(); DeserializeCamera(entityData["camera"], *ed->Camera); }
                    if (entityData.contains("terrain")) { if (!ed->Terrain) ed->Terrain = std::make_unique<TerrainComponent>(); DeserializeTerrain(entityData["terrain"], *ed->Terrain); }
                    if (entityData.contains("emitter")) { if (!ed->Emitter) ed->Emitter = std::make_unique<ParticleEmitterComponent>(); DeserializeParticleEmitter(entityData["emitter"], *ed->Emitter); }
                    if (entityData.contains("canvas")) { if (!ed->Canvas) ed->Canvas = std::make_unique<CanvasComponent>(); DeserializeCanvas(entityData["canvas"], *ed->Canvas); }
                    if (entityData.contains("panel")) { if (!ed->Panel) ed->Panel = std::make_unique<PanelComponent>(); DeserializePanel(entityData["panel"], *ed->Panel); }
                    if (entityData.contains("button")) { if (!ed->Button) ed->Button = std::make_unique<ButtonComponent>(); DeserializeButton(entityData["button"], *ed->Button); }
                    if (entityData.contains("scripts")) { DeserializeScripts(entityData["scripts"], ed->Scripts); }
                    if (entityData.contains("animator")) { if (!ed->AnimationPlayer) ed->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>(); DeserializeAnimator(entityData["animator"], *ed->AnimationPlayer); }
                    if (entityData.contains("skeleton")) { if (!ed->Skeleton) ed->Skeleton = std::make_unique<SkeletonComponent>(); DeserializeSkeleton(entityData["skeleton"], *ed->Skeleton); }
                    if (entityData.contains("skinning")) { if (!ed->Skinning) ed->Skinning = std::make_unique<SkinningComponent>(); DeserializeSkinning(entityData["skinning"], *ed->Skinning); }
                }
            });
        }
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
            auto itChild = idMapping.find(oldId);
            auto itParent = idMapping.find(oldParent);
            if (itChild != idMapping.end() && itParent != idMapping.end()) {
                EntityID childNew = itChild->second;
                EntityID parentNew = itParent->second;
                if (childNew == (EntityID)0 || childNew == (EntityID)-1) continue;
                if (parentNew == (EntityID)0 || parentNew == (EntityID)-1) {
                    std::cout << "[Parent] skip unresolved parent for childOld=" << oldId << " childNew=" << childNew << std::endl;
                    continue;
                }
                if (opaqueRoots.find(childNew) != opaqueRoots.end()) continue;
                if (opaqueRoots.find(parentNew) != opaqueRoots.end()) continue;
                scene.SetParent(childNew, parentNew);
            } else {
                std::cout << "[Parent] mapping missing for oldId=" << oldId << " or oldParent=" << oldParent << std::endl;
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
        // Collect and sort overrides by path depth so parents are processed before children
        struct OverrideItem { std::string relPath; const nlohmann::json* j; int depth; };
        std::vector<OverrideItem> items;
        for (const auto& childOverride : entityData["children"]) {
            if (!childOverride.contains("_modelNodePath")) continue;
            std::string relPath = childOverride["_modelNodePath"].get<std::string>();
            int depth = 0; for (char c : relPath) if (c=='/') ++depth;
            items.push_back({std::move(relPath), &childOverride, depth});
        }
        std::sort(items.begin(), items.end(), [](const OverrideItem& a, const OverrideItem& b){ return a.depth < b.depth; });

        auto resolveByPath = [&](const std::string& path) -> EntityID {
            EntityID target = rootNew;
            if (path.empty()) return target;
            std::stringstream ss(path); std::string part;
            while (std::getline(ss, part, '/')) {
                auto* d = scene.GetEntityData(target); if (!d) return (EntityID)-1;
                auto normalize = [](const std::string& name) -> std::string {
                    // strip trailing _<digits>
                    size_t us = name.find_last_of('_');
                    if (us == std::string::npos) return name;
                    bool digits = true; for (size_t i = us + 1; i < name.size(); ++i) { if (!std::isdigit(static_cast<unsigned char>(name[i]))) { digits = false; break; } }
                    if (!digits) return name;
                    return name.substr(0, us);
                };
                const std::string partNorm = normalize(part);
                EntityID next = (EntityID)-1;
                for (EntityID c : d->Children) {
                    auto* cd = scene.GetEntityData(c); if (!cd) continue;
                    const std::string childName = cd->Name;
                    if (childName == part || normalize(childName) == partNorm) { next = c; break; }
                }
                if (next == (EntityID)-1) return (EntityID)-1; target = next;
            }
            return target;
        };

        auto findByMeshFileId = [&](int fileID) -> EntityID {
            std::function<EntityID(EntityID)> dfs = [&](EntityID id)->EntityID{
                auto* d = scene.GetEntityData(id); if (!d) return (EntityID)-1;
                if (d->Mesh && d->Mesh->meshReference.fileID == fileID) return id;
                for (EntityID c : d->Children) { EntityID r = dfs(c); if (r != (EntityID)-1) return r; }
                return (EntityID)-1;
            };
            return dfs(rootNew);
        };

        for (const auto& it : items) {
            const auto& childOverride = *it.j;
            const std::string& relPath = it.relPath;
            EntityID target = resolveByPath(relPath);

            // Fallback: try to resolve by mesh fileID when path-based lookup fails (handles renamed nodes with meshes)
            if (target == (EntityID)-1 && childOverride.contains("mesh") && childOverride["mesh"].contains("fileID")) {
                int fid = childOverride["mesh"]["fileID"].get<int>();
                target = findByMeshFileId(fid);
            }

            if (target == (EntityID)-1) {
                // Heuristic: avoid creating duplicates for original model nodes.
                // Only create when the override clearly represents a user-added node
                // (e.g., has non-model components). If it looks like a model node (has mesh) or
                // contains only transform/name, skip creation.
                auto has = [&](const char* k){ return childOverride.contains(k); };
                bool looksUserAdded = has("camera") || has("light") || has("collider") || has("rigidbody") || has("staticbody")
                    || has("emitter") || has("canvas") || has("panel") || has("button") || has("scripts") || has("terrain")
                    || (has("animator") && !childOverride["animator"].is_null());
                bool looksModelNode = has("mesh");
                // Prefer updating an existing child with the same (normalized) name under the intended parent
                if (looksUserAdded) {
                    std::string parentPath;
                    std::string leafName;
                    auto ppos = relPath.find_last_of('/');
                    if (ppos != std::string::npos) { parentPath = relPath.substr(0, ppos); leafName = relPath.substr(ppos+1); }
                    else { leafName = relPath; }
                    EntityID parentTarget = resolveByPath(parentPath);
                    if (parentTarget != (EntityID)-1) {
                        auto* pd = scene.GetEntityData(parentTarget);
                        if (pd) {
                            auto normalize = [](const std::string& name)->std::string{
                                size_t us = name.find_last_of('_');
                                if (us == std::string::npos) return name;
                                bool digits = true; for (size_t i = us + 1; i < name.size(); ++i) { if (!std::isdigit(static_cast<unsigned char>(name[i]))) { digits = false; break; } }
                                return digits ? name.substr(0, us) : name;
                            };
                            const std::string leafNorm = normalize(leafName);
                            for (EntityID c : pd->Children) {
                                if (auto* cd = scene.GetEntityData(c)) {
                                    if (normalize(cd->Name) == leafNorm) { target = c; break; }
                                }
                            }
                        }
                    }
                }
                if (!looksUserAdded || looksModelNode) {
                    continue; // skip creating; assume it's an original model child we failed to resolve
                }
                // Treat as an added node: attach under parent path
                EntityID parentTarget = rootNew;
                std::string parentPath;
                std::string leafName;
                auto pos = relPath.find_last_of('/');
                if (pos != std::string::npos) { parentPath = relPath.substr(0, pos); leafName = relPath.substr(pos+1); }
                else { parentPath.clear(); leafName = relPath; }
                parentTarget = resolveByPath(parentPath);
                if (parentTarget == (EntityID)-1) continue;
                if (target != (EntityID)-1) {
                    // Apply override to the matched existing child
                    auto* td = scene.GetEntityData(target); if (!td) continue;
                    if (childOverride.contains("transform")) { DeserializeTransform(childOverride["transform"], td->Transform); td->Transform.TransformDirty = true; }
                    if (childOverride.contains("camera")) { if (!td->Camera) td->Camera = std::make_unique<CameraComponent>(); DeserializeCamera(childOverride["camera"], *td->Camera); }
                    if (childOverride.contains("light")) { if (!td->Light) td->Light = std::make_unique<LightComponent>(); DeserializeLight(childOverride["light"], *td->Light); }
                    if (childOverride.contains("collider")) { if (!td->Collider) td->Collider = std::make_unique<ColliderComponent>(); DeserializeCollider(childOverride["collider"], *td->Collider); }
                    if (childOverride.contains("rigidbody")) { if (!td->RigidBody) td->RigidBody = std::make_unique<RigidBodyComponent>(); DeserializeRigidBody(childOverride["rigidbody"], *td->RigidBody); }
                    if (childOverride.contains("staticbody")) { if (!td->StaticBody) td->StaticBody = std::make_unique<StaticBodyComponent>(); DeserializeStaticBody(childOverride["staticbody"], *td->StaticBody); }
                    if (childOverride.contains("emitter")) { if (!td->Emitter) td->Emitter = std::make_unique<ParticleEmitterComponent>(); DeserializeParticleEmitter(childOverride["emitter"], *td->Emitter); }
                    if (childOverride.contains("canvas")) { if (!td->Canvas) td->Canvas = std::make_unique<CanvasComponent>(); DeserializeCanvas(childOverride["canvas"], *td->Canvas); }
                    if (childOverride.contains("panel")) { if (!td->Panel) td->Panel = std::make_unique<PanelComponent>(); DeserializePanel(childOverride["panel"], *td->Panel); }
                    if (childOverride.contains("button")) { if (!td->Button) td->Button = std::make_unique<ButtonComponent>(); DeserializeButton(childOverride["button"], *td->Button); }
                    if (childOverride.contains("scripts")) { DeserializeScripts(childOverride["scripts"], td->Scripts); }
                    if (childOverride.contains("animator")) { if (!td->AnimationPlayer) td->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>(); DeserializeAnimator(childOverride["animator"], *td->AnimationPlayer); }
                    if (childOverride.contains("name")) { td->Name = childOverride["name"].get<std::string>(); }
                } else {
                    // Create entity from override json and parent it under parentTarget
                    nlohmann::json jcopy = childOverride;
                    if (!jcopy.contains("name")) jcopy["name"] = leafName;
                    EntityID newChild = DeserializeEntity(jcopy, scene);
                    if (newChild != 0 && newChild != (EntityID)-1) {
                        scene.SetParent(newChild, parentTarget);
                    }
                }
                continue;
            }

            // Apply overrides to existing node
            auto* td = scene.GetEntityData(target); if (!td) continue;
            if (childOverride.contains("transform")) { DeserializeTransform(childOverride["transform"], td->Transform); td->Transform.TransformDirty = true; }
            if (childOverride.contains("mesh")) {
                if (!td->Mesh) td->Mesh = std::make_unique<MeshComponent>();
                DeserializeMesh(childOverride["mesh"], *td->Mesh);
            }
            if (childOverride.contains("light")) {
                if (!td->Light) td->Light = std::make_unique<LightComponent>();
                DeserializeLight(childOverride["light"], *td->Light);
            }
            if (childOverride.contains("collider")) {
                if (!td->Collider) td->Collider = std::make_unique<ColliderComponent>();
                DeserializeCollider(childOverride["collider"], *td->Collider);
            }
            if (childOverride.contains("rigidbody")) {
                if (!td->RigidBody) td->RigidBody = std::make_unique<RigidBodyComponent>();
                DeserializeRigidBody(childOverride["rigidbody"], *td->RigidBody);
            }
            if (childOverride.contains("staticbody")) {
                if (!td->StaticBody) td->StaticBody = std::make_unique<StaticBodyComponent>();
                DeserializeStaticBody(childOverride["staticbody"], *td->StaticBody);
            }
            if (childOverride.contains("camera")) {
                if (!td->Camera) td->Camera = std::make_unique<CameraComponent>();
                DeserializeCamera(childOverride["camera"], *td->Camera);
            }
            if (childOverride.contains("terrain")) {
                if (!td->Terrain) td->Terrain = std::make_unique<TerrainComponent>();
                DeserializeTerrain(childOverride["terrain"], *td->Terrain);
            }
            if (childOverride.contains("emitter")) {
                if (!td->Emitter) td->Emitter = std::make_unique<ParticleEmitterComponent>();
                DeserializeParticleEmitter(childOverride["emitter"], *td->Emitter);
            }
            if (childOverride.contains("canvas")) {
                if (!td->Canvas) td->Canvas = std::make_unique<CanvasComponent>();
                DeserializeCanvas(childOverride["canvas"], *td->Canvas);
            }
            if (childOverride.contains("panel")) {
                if (!td->Panel) td->Panel = std::make_unique<PanelComponent>();
                DeserializePanel(childOverride["panel"], *td->Panel);
            }
            if (childOverride.contains("button")) {
                if (!td->Button) td->Button = std::make_unique<ButtonComponent>();
                DeserializeButton(childOverride["button"], *td->Button);
            }
            if (childOverride.contains("scripts")) { DeserializeScripts(childOverride["scripts"], td->Scripts); }
            if (childOverride.contains("animator")) {
                if (!td->AnimationPlayer) td->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>();
                DeserializeAnimator(childOverride["animator"], *td->AnimationPlayer);
            }
            if (childOverride.contains("name")) {
                td->Name = childOverride["name"].get<std::string>();
            }
        }
    }

    // Ensure transforms are dirty and updated after load
    for (const auto& entity : scene.GetEntities()) {
        scene.MarkTransformDirty(entity.GetID());
    }
    scene.UpdateTransforms();

    // -------------------------------------------------------------------------------------
    // Post-load de-duplication pass
    // Goal: eliminate accidental duplicates created during deserialization without touching
    //       instantiated model hierarchies (opaque roots and their descendants).
    // Currently handles Cameras (most common offender) using a structural signature.
    // -------------------------------------------------------------------------------------
    {
        // Build the protected set = opaque roots + all their descendants
        std::unordered_set<EntityID> protectedIds;
        auto addDescendants = [&](auto&& self, EntityID id) -> void {
            if (protectedIds.count(id)) return;
            protectedIds.insert(id);
            if (auto* d = scene.GetEntityData(id)) {
                for (EntityID c : d->Children) self(self, c);
            }
        };
        for (EntityID root : opaqueRoots) addDescendants(addDescendants, root);

        auto normalizeName = [](const std::string& name) -> std::string {
            size_t us = name.find_last_of('_');
            if (us == std::string::npos) return name;
            bool digits = true;
            for (size_t i = us + 1; i < name.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(name[i]))) { digits = false; break; }
            }
            return digits ? name.substr(0, us) : name;
        };

        auto round3 = [](float v) -> int { return static_cast<int>(std::round(v * 1000.0f)); };

        struct SignatureKey {
            std::string key;
            EntityID keptId = (EntityID)-1;
        };

        std::unordered_map<std::string, EntityID> signatureToEntity;
        std::vector<EntityID> entitiesToRemove;

        for (const auto& e : scene.GetEntities()) {
            EntityID id = e.GetID();
            if (protectedIds.count(id)) continue; // never dedup model-instantiated hierarchies
            auto* d = scene.GetEntityData(id);
            if (!d) continue;

            // Only dedup Cameras for now
            if (!d->Camera) continue;

            // Build a structural signature of the camera + transform
            const auto& cam = *d->Camera;
            const auto& t = d->Transform;
            std::ostringstream oss;
            oss << "type=camera";
            oss << "|name=" << normalizeName(d->Name);
            oss << "|layer=" << d->Layer << "|tag=" << d->Tag;
            oss << "|active=" << cam.Active << "|prio=" << cam.priority
                << "|fov=" << round3(cam.FieldOfView) << "|near=" << round3(cam.NearClip)
                << "|far=" << round3(cam.FarClip) << "|persp=" << cam.IsPerspective;
            // Position/rotation/scale rounded to avoid float noise
            oss << "|px=" << round3(t.Position.x) << "|py=" << round3(t.Position.y) << "|pz=" << round3(t.Position.z);
            oss << "|rx=" << round3(t.Rotation.x) << "|ry=" << round3(t.Rotation.y) << "|rz=" << round3(t.Rotation.z);
            oss << "|sx=" << round3(t.Scale.x) << "|sy=" << round3(t.Scale.y) << "|sz=" << round3(t.Scale.z);

            std::string sig = oss.str();
            auto it = signatureToEntity.find(sig);
            if (it == signatureToEntity.end()) {
                signatureToEntity.emplace(std::move(sig), id);
            } else {
                // Duplicate found: remove the later one
                entitiesToRemove.push_back(id);
            }
        }

        // Remove duplicates after iteration
        for (EntityID rid : entitiesToRemove) {
            scene.RemoveEntity(rid);
        }
    }

    try {
        // Dump GUID -> hierarchy path map
        auto computePath = [&](EntityID id) -> std::string {
            std::vector<std::string> parts;
            EntityID cur = id;
            size_t guard = 0;
            while (cur != (EntityID)-1 && guard++ < 100000) {
                auto* d = scene.GetEntityData(cur);
                if (!d) break;
                parts.push_back(d->Name);
                if (d->Parent == (EntityID)-1) break;
                cur = d->Parent;
            }
            std::reverse(parts.begin(), parts.end());
            std::string s; for (size_t i=0;i<parts.size();++i){ s += parts[i]; if (i+1<parts.size()) s += "/"; }
            return s;
        };
        for (const auto& e : scene.GetEntities()) {
            auto* d = scene.GetEntityData(e.GetID()); if (!d) continue;
            std::cout << "[Hierarchy] guid=" << d->EntityGuid.ToString() << " id=" << e.GetID() << " path=" << computePath(e.GetID()) << std::endl;
        }
        std::cout << "[DeserializeEnd] entities=" << scene.GetEntities().size() << std::endl;
    } catch(...) {}

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
        // Virtual filesystem first; no direct OS reads for runtime
        {
            std::string sceneText;
            if (FileSystem::Instance().ReadTextFile(filepath, sceneText)) {
                sceneData = json::parse(sceneText);
            } else {
                std::vector<uint8_t> bytes;
                if (FileSystem::Instance().ReadFile(filepath, bytes)) {
                    sceneData = json::parse(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
                } else {
                    std::cerr << "[Serializer] Scene file does not exist or cannot be read: " << filepath << std::endl;
                    return false;
                }
            }
        }
        const std::string version = sceneData.value("version", "");
        std::cout << "[SceneLoad] Version=" << version << " Entities=" << (sceneData.contains("entities") ? sceneData["entities"].size() : 0) << std::endl;
        
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
    
    // Create a temporary entity to serialize (simplified direct copy of components)
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
    if (entityData.Skeleton) {
        entityJson["skeleton"] = SerializeSkeleton(*entityData.Skeleton);
    }
    if (entityData.Skinning) {
        entityJson["skinning"] = SerializeSkinning(*entityData.Skinning);
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
    if (entityJson.contains("skeleton")) {
        entityData.Skeleton = std::make_unique<SkeletonComponent>();
        DeserializeSkeleton(entityJson["skeleton"], *entityData.Skeleton);
    }
    if (entityJson.contains("skinning")) {
        entityData.Skinning = std::make_unique<SkinningComponent>();
        DeserializeSkinning(entityJson["skinning"], *entityData.Skinning);
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
        json prefabData;
        std::string text;
        if (FileSystem::Instance().ReadTextFile(filepath, text)) {
            prefabData = json::parse(text);
        } else {
            std::vector<uint8_t> bytes;
            if (FileSystem::Instance().ReadFile(filepath, bytes)) {
                prefabData = json::parse(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
            } else {
                std::cerr << "[Serializer] Prefab file does not exist or cannot be read: " << filepath << std::endl;
                return false;
            }
        }
        
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

// New: serialize an entity and all its descendants as a prefab subtree
json Serializer::SerializePrefabSubtree(EntityID rootId, Scene& scene) {
    json prefab;
    prefab["version"] = "2.0";
    prefab["type"] = "prefab";
    prefab["entities"] = json::array();
    if (!scene.GetEntityData(rootId)) return prefab;

    // Collect subtree ids in DFS order
    std::vector<EntityID> order;
    std::function<void(EntityID)> dfs = [&](EntityID id){
        order.push_back(id);
        if (auto* d = scene.GetEntityData(id)) {
            for (EntityID c : d->Children) dfs(c);
        }
    };
    dfs(rootId);

    // Identify imported model roots within the subtree and collect per-node overrides; skip their descendants
    std::unordered_set<EntityID> skip;
    std::unordered_map<EntityID, nlohmann::json> rootOverrides;
    auto computeNodePath = [&](EntityID root, EntityID node) -> std::string {
        std::vector<std::string> parts; EntityID cur = node;
        while (cur != -1) {
            auto* d = scene.GetEntityData(cur); if (!d) break;
            parts.push_back(d->Name);
            if (cur == root) break;
            cur = d->Parent;
        }
        std::reverse(parts.begin(), parts.end());
        if (!parts.empty()) parts.erase(parts.begin()); // make path relative to model root
        std::string s; for (size_t i=0;i<parts.size();++i){ s += parts[i]; if (i+1<parts.size()) s += "/"; }
        return s;
    };
    for (EntityID id : order) {
        std::string path; ClaymoreGUID g{};
        if (IsImportedModelRoot(scene, id, path, g)) {
            // Collect overrides for children under this model root
            rootOverrides[id] = nlohmann::json::array();
            std::function<void(EntityID)> walk = [&](EntityID cur){
                auto* d = scene.GetEntityData(cur); if (!d) return;
                for (EntityID c : d->Children) {
                    skip.insert(c);
                    nlohmann::json childJ = SerializeEntity(c, scene);
                    childJ["_modelNodePath"] = computeNodePath(id, c);
                    // Keep name; drop relational/id-only fields
                    childJ.erase("id"); childJ.erase("parent"); childJ.erase("children"); childJ.erase("asset");
                    if (!childJ.empty()) rootOverrides[id].push_back(std::move(childJ));
                    walk(c);
                }
            };
            walk(id);
            skip.erase(id);
        }
    }

    // Build emission list excluding skipped nodes
    std::vector<EntityID> emit;
    emit.reserve(order.size());
    for (EntityID id : order) {
        if (skip.find(id) == skip.end()) emit.push_back(id);
    }
    std::unordered_map<EntityID, int> idToEmitIndex;
    for (int i = 0; i < (int)emit.size(); ++i) idToEmitIndex[emit[i]] = i;

    // Emit compact subtree
    for (int i = 0; i < (int)emit.size(); ++i) {
        EntityID eid = emit[i];
        json e = SerializeEntity(eid, scene);
        e.erase("id");
        e.erase("guid");
        // Parent index among emitted nodes only
        int parentIndex = -1;
        if (auto* d = scene.GetEntityData(eid)) {
            if (d->Parent != (EntityID)-1) {
                auto it = idToEmitIndex.find(d->Parent);
                if (it != idToEmitIndex.end()) parentIndex = it->second;
            }
        }
        e["parentIndex"] = parentIndex;
        e.erase("children");
        // Attach asset compact record and collected overrides if this was a model root
        std::string modelPath; ClaymoreGUID guid{};
        if (IsImportedModelRoot(scene, eid, modelPath, guid)) {
            json asset;
            asset["type"] = "model";
            std::string v = modelPath; for(char& c: v) if (c=='\\') c='/';
            auto pos = v.find("assets/"); if (pos != std::string::npos) v = v.substr(pos);
            asset["path"] = v;
            asset["guid"] = guid.ToString();
            e["asset"] = std::move(asset);
            auto it = rootOverrides.find(eid);
            e["children"] = (it != rootOverrides.end()) ? it->second : nlohmann::json::array();
        }
        prefab["entities"].push_back(std::move(e));
    }
    return prefab;
}

bool Serializer::SavePrefabSubtreeToFile(Scene& scene, EntityID rootId, const std::string& filepath) {
    try {
        json j = SerializePrefabSubtree(rootId, scene);
        fs::path p(filepath);
        fs::create_directories(p.parent_path());
        std::ofstream out(filepath);
        if (!out) return false;
        out << j.dump(4);
        out.close();
        std::cout << "[Serializer] Prefab subtree saved to: " << filepath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Serializer] Error saving prefab subtree: " << e.what() << std::endl;
        return false;
    }
}

EntityID Serializer::LoadPrefabToScene(const std::string& filepath, Scene& scene) {
    try {
        json data;
        std::string text;
        if (FileSystem::Instance().ReadTextFile(filepath, text)) {
            data = json::parse(text);
        } else {
            std::vector<uint8_t> bytes;
            if (FileSystem::Instance().ReadFile(filepath, bytes)) {
                data = json::parse(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
            } else {
                std::cerr << "[Serializer] Prefab file does not exist or cannot be read: " << filepath << std::endl;
                return (EntityID)-1;
            }
        }
        // Support both legacy and subtree formats
        if (data.contains("entities") && data["entities"].is_array()) {
            // Subtree format: instantiate compact asset nodes (models) and create pure-serialized nodes; then apply overrides.
            const auto& ents = data["entities"];
            std::vector<EntityID> idxToNew(ents.size(), (EntityID)-1);
            std::unordered_set<EntityID> opaqueRoots; // model-instantiated roots that carry their own hierarchy

            // First pass: create entities or instantiate models
            for (size_t i = 0; i < ents.size(); ++i) {
                const json& je = ents[i];
                // Model asset node?
                if (je.contains("asset") && je["asset"].is_object()) {
                    const auto& a = je["asset"];
                    std::string type = a.value("type", "");
                    if (type == "model") {
                        // Resolve path relative to project
                        std::string p = a.value("path", "");
                        std::string resolved = p;
                        if (!resolved.empty() && !fs::exists(resolved)) resolved = (Project::GetProjectDirectory() / p).string();
                        for (char& c : resolved) if (c=='\\') c = '/';
                        // Register GUID mapping hint if present
                        try {
                            std::string gstr = a.value("guid", "");
                            if (!gstr.empty()) {
                                ClaymoreGUID g = ClaymoreGUID::FromString(gstr);
                                if (!(g.high == 0 && g.low == 0)) {
                                    std::string v = p; for (char& ch : v) if (ch=='\\') ch = '/';
                                    AssetLibrary::Instance().RegisterAsset(AssetReference(g, 0, (int)AssetType::Mesh), AssetType::Mesh, v, v);
                                    if (!resolved.empty()) AssetLibrary::Instance().RegisterPathAlias(g, resolved);
                                }
                            }
                        } catch(...) {}
                        // Determine spawn position
                        glm::vec3 pos(0.0f);
                        if (je.contains("transform")) { auto t = je["transform"]; if (t.contains("position")) pos = DeserializeVec3(t["position"]); }
                        // Prefer fast path via .meta next to model
                        std::string metaTry = resolved; std::string ext = fs::path(resolved).extension().string(); std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext != ".meta") { fs::path rp(resolved); fs::path mp = rp.parent_path() / (rp.stem().string() + ".meta"); if (fs::exists(mp)) metaTry = mp.string(); }
                        EntityID nid = (EntityID)-1;
                        if (!metaTry.empty() && fs::path(metaTry).extension() == ".meta") {
                            nid = scene.InstantiateModelFast(metaTry, pos);
                            if (nid == (EntityID)0 || nid == (EntityID)-1) nid = scene.InstantiateModel(resolved, pos);
                        } else {
                            nid = scene.InstantiateModel(resolved, pos);
                        }
                        if (nid != (EntityID)-1 && nid != (EntityID)0) {
                            idxToNew[i] = nid;
                            opaqueRoots.insert(nid);
                            // Apply transform, scripts, animator on the root
                            if (auto* ed = scene.GetEntityData(nid)) {
                                if (je.contains("name")) { ed->Name = je["name"].get<std::string>(); }
                                if (je.contains("transform")) DeserializeTransform(je["transform"], ed->Transform);
                                if (je.contains("scripts")) DeserializeScripts(je["scripts"], ed->Scripts);
                                if (je.contains("animator")) { if (!ed->AnimationPlayer) ed->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>(); DeserializeAnimator(je["animator"], *ed->AnimationPlayer); }
                            }
                        }
                        continue; // done with this entry
                    }
                }

                // Regular serialized node
                std::string name = je.value("name", "Entity");
                Entity e = scene.CreateEntityExact(name);
                EntityID nid = e.GetID();
                idxToNew[i] = nid;
                auto* d = scene.GetEntityData(nid);
                if (!d) continue;
                if (je.contains("layer")) d->Layer = je["layer"];
                if (je.contains("tag")) d->Tag = je["tag"];
                if (je.contains("transform")) DeserializeTransform(je["transform"], d->Transform);
                if (je.contains("mesh")) {
                    if (!d->Mesh) d->Mesh = std::make_unique<MeshComponent>();
                    // Prefer unified builder over legacy deserializer
                    ClaymoreGUID meshGuid{}; int fileId = 0; ClaymoreGUID skelGuid{};
                    try {
                        if (je["mesh"].contains("meshReference")) { AssetReference tmp; je["mesh"]["meshReference"].get_to(tmp); meshGuid = tmp.guid; fileId = tmp.fileID; }
                    } catch(...) {}
                    try { if (je.contains("skeleton") && je["skeleton"].contains("skeletonGuid")) je["skeleton"]["skeletonGuid"].get_to(skelGuid); } catch(...) {}
                    BuildModelParams bp{ meshGuid, fileId, skelGuid, nullptr, nid, &scene };
                    BuildResult br = BuildRendererFromAssets(bp);
                    if (!br.ok) {
                        std::cerr << "[Serializer] ERROR: Prefab node renderer build failed for entity '" << d->Name << "'" << std::endl;
                    }
                }
                if (je.contains("light")) { d->Light = std::make_unique<LightComponent>(); DeserializeLight(je["light"], *d->Light); }
                if (je.contains("collider")) { d->Collider = std::make_unique<ColliderComponent>(); DeserializeCollider(je["collider"], *d->Collider); }
                if (je.contains("rigidbody")) { d->RigidBody = std::make_unique<RigidBodyComponent>(); DeserializeRigidBody(je["rigidbody"], *d->RigidBody); }
                if (je.contains("staticbody")) { d->StaticBody = std::make_unique<StaticBodyComponent>(); DeserializeStaticBody(je["staticbody"], *d->StaticBody); }
                if (je.contains("camera")) { d->Camera = std::make_unique<CameraComponent>(); DeserializeCamera(je["camera"], *d->Camera); }
                if (je.contains("terrain")) { d->Terrain = std::make_unique<TerrainComponent>(); DeserializeTerrain(je["terrain"], *d->Terrain); }
                if (je.contains("emitter")) { d->Emitter = std::make_unique<ParticleEmitterComponent>(); DeserializeParticleEmitter(je["emitter"], *d->Emitter); }
                if (je.contains("canvas")) { d->Canvas = std::make_unique<CanvasComponent>(); DeserializeCanvas(je["canvas"], *d->Canvas); }
                if (je.contains("panel")) { d->Panel = std::make_unique<PanelComponent>(); DeserializePanel(je["panel"], *d->Panel); }
                if (je.contains("button")) { d->Button = std::make_unique<ButtonComponent>(); DeserializeButton(je["button"], *d->Button); }
                if (je.contains("scripts")) { DeserializeScripts(je["scripts"], d->Scripts); }
                if (je.contains("animator")) { if (!d->AnimationPlayer) d->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>(); DeserializeAnimator(je["animator"], *d->AnimationPlayer); }
                if (je.contains("skeleton")) { if (!d->Skeleton) d->Skeleton = std::make_unique<SkeletonComponent>(); DeserializeSkeleton(je["skeleton"], *d->Skeleton); }
                if (je.contains("skinning")) { if (!d->Skinning) d->Skinning = std::make_unique<SkinningComponent>(); DeserializeSkinning(je["skinning"], *d->Skinning); }
            }

            // Second pass: parent fixup (skip opaque roots)
            for (size_t i = 0; i < ents.size(); ++i) {
                const json& je = ents[i];
                EntityID nid = idxToNew[i];
                if (nid == (EntityID)-1) continue;
                if (je.contains("parentIndex")) {
                    int pidx = je["parentIndex"].get<int>();
                    if (pidx >= 0 && pidx < (int)idxToNew.size()) {
                        EntityID pid = idxToNew[pidx];
                        if (pid != (EntityID)-1 && opaqueRoots.find(nid) == opaqueRoots.end() && opaqueRoots.find(pid) == opaqueRoots.end()) {
                            scene.SetParent(nid, pid);
                        }
                    }
                }
            }

            // Apply per-node overrides under compact model roots
            auto resolveByPath = [&](EntityID rootNew, const std::string& path) -> EntityID {
                EntityID target = rootNew;
                if (path.empty()) return target;
                std::stringstream ss(path); std::string part;
                auto normalize = [](const std::string& name) -> std::string {
                    size_t us = name.find_last_of('_');
                    if (us == std::string::npos) return name;
                    bool digits = true; for (size_t i = us + 1; i < name.size(); ++i) { if (!std::isdigit(static_cast<unsigned char>(name[i]))) { digits = false; break; } }
                    return digits ? name.substr(0, us) : name;
                };
                while (std::getline(ss, part, '/')) {
                    auto* d = scene.GetEntityData(target); if (!d) return (EntityID)-1;
                    const std::string partNorm = normalize(part);
                    EntityID next = (EntityID)-1;
                    for (EntityID c : d->Children) { auto* cd = scene.GetEntityData(c); if (!cd) continue; const std::string childName = cd->Name; if (childName == part || normalize(childName) == partNorm) { next = c; break; } }
                    if (next == (EntityID)-1) return (EntityID)-1; target = next;
                }
                return target;
            };
            auto findByMeshFileId = [&](EntityID rootNew, int fileID) -> EntityID {
                std::function<EntityID(EntityID)> dfs = [&](EntityID id)->EntityID{
                    auto* d = scene.GetEntityData(id); if (!d) return (EntityID)-1;
                    if (d->Mesh && d->Mesh->meshReference.fileID == fileID) return id;
                    for (EntityID c : d->Children) { EntityID r = dfs(c); if (r != (EntityID)-1) return r; }
                    return (EntityID)-1;
                };
                return dfs(rootNew);
            };

            for (size_t i = 0; i < ents.size(); ++i) {
                const json& je = ents[i];
                if (!(je.contains("asset") && je["asset"].is_object())) continue;
                const auto& a = je["asset"]; if (a.value("type", "") != std::string("model")) continue;
                EntityID rootNew = idxToNew[i]; if (rootNew == (EntityID)-1) continue;
                if (!je.contains("children") || !je["children"].is_array()) continue;
                // Sort overrides by depth so parents first
                struct OverrideItem { std::string relPath; const nlohmann::json* j; int depth; };
                std::vector<OverrideItem> items;
                for (const auto& childOverride : je["children"]) {
                    if (!childOverride.contains("_modelNodePath")) continue;
                    std::string relPath = childOverride["_modelNodePath"].get<std::string>();
                    int depth = 0; for (char c : relPath) if (c=='/') ++depth;
                    items.push_back({std::move(relPath), &childOverride, depth});
                }
                std::sort(items.begin(), items.end(), [](const OverrideItem& a, const OverrideItem& b){ return a.depth < b.depth; });

                for (const auto& it : items) {
                    const auto& childOverride = *it.j;
                    const std::string& relPath = it.relPath;
                    EntityID target = resolveByPath(rootNew, relPath);
                    if (target == (EntityID)-1 && childOverride.contains("mesh") && childOverride["mesh"].contains("fileID")) {
                        int fid = childOverride["mesh"]["fileID"].get<int>();
                        target = findByMeshFileId(rootNew, fid);
                    }
                    if (target == (EntityID)-1) continue;
                    auto* td = scene.GetEntityData(target); if (!td) continue;
                    if (childOverride.contains("transform")) { DeserializeTransform(childOverride["transform"], td->Transform); td->Transform.TransformDirty = true; }
                    if (childOverride.contains("mesh")) {
                        if (!td->Mesh) td->Mesh = std::make_unique<MeshComponent>();
                        ClaymoreGUID meshGuid{}; int fileId = 0; ClaymoreGUID skelGuid{};
                        try {
                            if (childOverride["mesh"].contains("meshReference")) { AssetReference tmp; childOverride["mesh"]["meshReference"].get_to(tmp); meshGuid = tmp.guid; fileId = tmp.fileID; }
                        } catch(...) {}
                        try { if (childOverride.contains("skeleton") && childOverride["skeleton"].contains("skeletonGuid")) childOverride["skeleton"]["skeletonGuid"].get_to(skelGuid); } catch(...) {}
                        BuildModelParams bp{ meshGuid, fileId, skelGuid, nullptr, target, &scene };
                        BuildResult br = BuildRendererFromAssets(bp);
                        if (!br.ok) {
                            std::cerr << "[Serializer] ERROR: Override renderer build failed at path under model root." << std::endl;
                        }
                    }
                    if (childOverride.contains("light")) { if (!td->Light) td->Light = std::make_unique<LightComponent>(); DeserializeLight(childOverride["light"], *td->Light); }
                    if (childOverride.contains("collider")) { if (!td->Collider) td->Collider = std::make_unique<ColliderComponent>(); DeserializeCollider(childOverride["collider"], *td->Collider); }
                    if (childOverride.contains("rigidbody")) { if (!td->RigidBody) td->RigidBody = std::make_unique<RigidBodyComponent>(); DeserializeRigidBody(childOverride["rigidbody"], *td->RigidBody); }
                    if (childOverride.contains("staticbody")) { if (!td->StaticBody) td->StaticBody = std::make_unique<StaticBodyComponent>(); DeserializeStaticBody(childOverride["staticbody"], *td->StaticBody); }
                    if (childOverride.contains("camera")) { if (!td->Camera) td->Camera = std::make_unique<CameraComponent>(); DeserializeCamera(childOverride["camera"], *td->Camera); }
                    if (childOverride.contains("terrain")) { if (!td->Terrain) td->Terrain = std::make_unique<TerrainComponent>(); DeserializeTerrain(childOverride["terrain"], *td->Terrain); }
                    if (childOverride.contains("emitter")) { if (!td->Emitter) td->Emitter = std::make_unique<ParticleEmitterComponent>(); DeserializeParticleEmitter(childOverride["emitter"], *td->Emitter); }
                    if (childOverride.contains("canvas")) { if (!td->Canvas) td->Canvas = std::make_unique<CanvasComponent>(); DeserializeCanvas(childOverride["canvas"], *td->Canvas); }
                    if (childOverride.contains("panel")) { if (!td->Panel) td->Panel = std::make_unique<PanelComponent>(); DeserializePanel(childOverride["panel"], *td->Panel); }
                    if (childOverride.contains("button")) { if (!td->Button) td->Button = std::make_unique<ButtonComponent>(); DeserializeButton(childOverride["button"], *td->Button); }
                    if (childOverride.contains("scripts")) { DeserializeScripts(childOverride["scripts"], td->Scripts); }
                    if (childOverride.contains("animator")) { if (!td->AnimationPlayer) td->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>(); DeserializeAnimator(childOverride["animator"], *td->AnimationPlayer); }
                    if (childOverride.contains("name")) { td->Name = childOverride["name"].get<std::string>(); }
                }
            }

            // Post-fixups: skeleton links for any serialized skel/skinning nodes created directly (non-opaque areas)
            for (EntityID nid : idxToNew) {
                if (nid == (EntityID)-1) continue;
                auto* d = scene.GetEntityData(nid);
                if (!d) continue;
                if (d->Skinning && d->Mesh) {
                    if (!std::dynamic_pointer_cast<SkinnedPBRMaterial>(d->Mesh->material)) {
                        d->Mesh->material = MaterialManager::Instance().CreateSkinnedPBRMaterial();
                    }
                }
                if (d->Skinning && d->Skinning->SkeletonRoot == (EntityID)-1) {
                    EntityID cur = nid; EntityID found = (EntityID)-1; size_t guard = 0;
                    while (cur != (EntityID)-1 && guard++ < 100000) { auto* cd = scene.GetEntityData(cur); if (cd && cd->Skeleton) { found = cur; break; } if (!cd) break; cur = cd->Parent; }
                    d->Skinning->SkeletonRoot = found;
                }
            }

            EntityID rootNew = idxToNew.empty() ? (EntityID)-1 : idxToNew[0];
            return rootNew;
        }
        // Legacy single-entity format
        EntityData ed;
        if (!DeserializePrefab(data, ed, scene)) return (EntityID)-1;
        Entity e = scene.CreateEntity(ed.Name.empty() ? "Prefab" : ed.Name);
        EntityData* dst = scene.GetEntityData(e.GetID());
        if (!dst) return (EntityID)-1;
        *dst = ed.DeepCopy(e.GetID(), &scene);
        // Fix up legacy single-entity prefab: resolve skeleton links and skinnings
        if (dst->Skinning && dst->Skinning->SkeletonRoot == (EntityID)-1) {
            // Anchor to self if this entity has a skeleton
            if (dst->Skeleton) dst->Skinning->SkeletonRoot = e.GetID();
        }
        if (dst->Skeleton) {
            // Rebuild BoneEntities by name within this entity subtree
            std::unordered_map<std::string, EntityID> nameToId;
            std::function<void(EntityID)> build = [&](EntityID id){
                auto* ed2 = scene.GetEntityData(id); if (!ed2) return;
                nameToId[ed2->Name] = id; for (EntityID c : ed2->Children) build(c);
            };
            build(e.GetID());
            const size_t n = dst->Skeleton->InverseBindPoses.size();
            dst->Skeleton->BoneEntities.assign(n, (EntityID)-1);
            std::vector<std::string> boneNames(n);
            for (const auto& kv : dst->Skeleton->BoneNameToIndex) { int idx = kv.second; if (idx >= 0 && (size_t)idx < n) boneNames[(size_t)idx] = kv.first; }
            for (size_t i = 0; i < n; ++i) { const std::string& nm = boneNames[i]; if (nm.empty()) continue; auto it = nameToId.find(nm); if (it != nameToId.end()) dst->Skeleton->BoneEntities[i] = it->second; }
        }
        return e.GetID();
    } catch (const std::exception& e) {
        std::cerr << "[Serializer] Error loading prefab to scene: " << e.what() << std::endl;
        return (EntityID)-1;
    }
}