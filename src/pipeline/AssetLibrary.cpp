#include "AssetLibrary.h"
#include "AssetRegistry.h"
#include "rendering/StandardMeshManager.h"
#include "rendering/MaterialManager.h"
#include "rendering/ModelLoader.h"
#include "rendering/TextureLoader.h"
#include "serialization/Serializer.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include "editor/Project.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace fs = std::filesystem;

void AssetLibrary::RegisterAsset(const AssetReference& ref, AssetType type, const std::string& path, const std::string& name) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    // Normalize forward slashes
    std::string normPath = path;
    std::replace(normPath.begin(), normPath.end(), '\\', '/');

    // If already registered with same mapping, skip noisy log and return
    auto it = m_Assets.find(ref.guid);
    if (it != m_Assets.end()) {
        bool samePath = (it->second.path == normPath);
        bool sameType = (it->second.type == type);
        if (samePath && sameType) {
            // Ensure reverse maps are consistent then exit quietly
            m_PathToGUID[normPath] = ref.guid;
            m_GUIDToPath[ref.guid] = normPath;
            return;
        }
        // If GUID exists but path changed (rename/move), update and keep entry data (like cached meshes)
        it->second.path = normPath;
        it->second.type = type;
        it->second.name = name;
        m_PathToGUID[normPath] = ref.guid;
        m_GUIDToPath[ref.guid] = normPath;
        std::cout << "[AssetLibrary] Updated asset path: " << name << " (GUID: " << ref.guid.ToString() << ") -> " << normPath << std::endl;
        return;
    }

    AssetEntry entry(ref, type, normPath, name);
    m_Assets[ref.guid] = entry;
    m_PathToGUID[normPath] = ref.guid;
    m_GUIDToPath[ref.guid] = normPath;

    std::cout << "[AssetLibrary] Registered asset: " << name << " (GUID: " << ref.guid.ToString() << ")" << std::endl;
}

void AssetLibrary::RegisterPathAlias(const ClaymoreGUID& guid, const std::string& altPath) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    if (guid.high == 0 && guid.low == 0) return;
    std::string norm = altPath;
    std::replace(norm.begin(), norm.end(), '\\', '/');
    m_PathToGUID[norm] = guid;
}

void AssetLibrary::UnregisterAsset(const AssetReference& ref) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    auto it = m_Assets.find(ref.guid);
    if (it != m_Assets.end()) {
        m_PathToGUID.erase(it->second.path);
        m_GUIDToPath.erase(ref.guid);
        m_Assets.erase(it);
    }
}

AssetEntry* AssetLibrary::GetAsset(const AssetReference& ref) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    auto it = m_Assets.find(ref.guid);
    return it != m_Assets.end() ? &it->second : nullptr;
}

AssetEntry* AssetLibrary::GetAsset(const ClaymoreGUID& guid) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    auto it = m_Assets.find(guid);
    return it != m_Assets.end() ? &it->second : nullptr;
}

AssetEntry* AssetLibrary::GetAsset(const std::string& path) {
    // Normalize slashes first (no lock needed)
    std::string norm = path;
    std::replace(norm.begin(), norm.end(), '\\', '/');

    // Use helper which handles locking internally
    ClaymoreGUID guid = GetGUIDForPath(norm);
    if (guid.high != 0 || guid.low != 0) {
        // This overload also locks internally
        return GetAsset(guid);
    }

    // Fallback: check direct map under lock
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        auto it = m_PathToGUID.find(norm);
        if (it != m_PathToGUID.end()) {
            auto it2 = m_Assets.find(it->second);
            return it2 != m_Assets.end() ? &it2->second : nullptr;
        }
    }
    return nullptr;
}

bool AssetLibrary::LoadPrefabIntoEntity(const AssetReference& ref, EntityData& outEntity, Scene& scene) {
    AssetEntry* entry = this->GetAsset(ref);
    if (!entry) return false;
    if (entry->type != AssetType::Prefab) return false;
    // Prefer new authoring prefab JSON: load minimal and inject via PrefabAPI when available.
    // For now, keep legacy fallback for compatibility.
    try {
        std::ifstream in(entry->path);
        if (in) {
            nlohmann::json j; in >> j; in.close();
            if (j.is_object() && j.contains("guid") && j.contains("entities")) {
                // Create a placeholder entity with the prefab name
                outEntity.Name = j.value("name", std::string("Prefab"));
                return true;
            }
        }
    } catch(...) {}
    return Serializer::LoadPrefabFromFile(entry->path, outEntity, scene);
}

std::shared_ptr<Mesh> AssetLibrary::LoadMesh(const AssetReference& ref) {
    AssetEntry* entry = GetAsset(ref);
    if (!entry) {
        std::cout << "[AssetLibrary] Warning: Asset not found for GUID: " << ref.guid.ToString() << std::endl;
        return nullptr;
    }

    // Primitives are a special GUID with per-type name
    if (ref.guid == AssetReference::CreatePrimitive("").guid) {
        // Return cached or create primitive mesh by name
        if (!entry->mesh) entry->mesh = CreatePrimitiveMesh(entry->name);
        return entry->mesh;
    }

    // For imported models, support submesh selection via fileID
    if (!entry->path.empty()) {
        // Fast-path: .meshbin or .meta
        std::string ext = std::filesystem::path(entry->path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".meshbin") {
            return LoadMeshBin(entry->path, ref.fileID);
        }
        if (ext == ".meta") {
            std::string meshBin = ResolveMeshBinFromMeta(entry->path, ref.fileID);
            if (!meshBin.empty()) return LoadMeshBin(meshBin, ref.fileID);
        }
        // Cache per-fileID meshes inside the entry by reusing its mesh pointer for fileID 0
        // and loading a model once per request; keep a simple cache map local static for now.
        struct Cache { std::mutex m; std::unordered_map<std::string, std::vector<std::shared_ptr<Mesh>>> map; };
        static Cache s_cache;
        std::vector<std::shared_ptr<Mesh>> meshListLocal;
        {
            std::lock_guard<std::mutex> lk(s_cache.m);
            auto& meshList = s_cache.map[entry->path];
            if (meshList.empty()) {
                // Load outside lock
            } else {
                meshListLocal = meshList;
            }
        }
        if (meshListLocal.empty()) {
            Model model = ModelLoader::LoadModel(entry->path);
            meshListLocal = model.Meshes;
            std::lock_guard<std::mutex> lk2(s_cache.m);
            s_cache.map[entry->path] = meshListLocal;
        }
        int idx = std::max(0, ref.fileID);
        if (idx < (int)meshListLocal.size()) {
            return meshListLocal[idx];
        }
        std::cout << "[AssetLibrary] Warning: fileID " << ref.fileID << " out of range for model: " << entry->path << std::endl;
        return meshListLocal.empty() ? nullptr : meshListLocal[0];
    }

    return nullptr;
}

std::shared_ptr<Material> AssetLibrary::LoadMaterial(const AssetReference& ref) {
    // Double-checked locking: avoid holding the mutex during heavy work
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        auto it = m_Assets.find(ref.guid);
        if (it == m_Assets.end()) return nullptr;
        if (it->second.material) return it->second.material;
    }
    auto created = MaterialManager::Instance().CreateSceneDefaultMaterial(&Scene::Get());
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        auto it = m_Assets.find(ref.guid);
        if (it == m_Assets.end()) return created;
        if (!it->second.material) it->second.material = created;
        return it->second.material;
    }
}

std::shared_ptr<bgfx::TextureHandle> AssetLibrary::LoadTexture(const AssetReference& ref) {
    std::string path;
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        auto it = m_Assets.find(ref.guid);
        if (it == m_Assets.end()) return nullptr;
        if (it->second.texture) return it->second.texture;
        path = it->second.path;
    }
    if (!path.empty()) {
        bgfx::TextureHandle handle = TextureLoader::Load2D(path);
        auto texPtr = std::make_shared<bgfx::TextureHandle>(handle);
        std::lock_guard<std::mutex> lk2(m_Mutex);
        auto it = m_Assets.find(ref.guid);
        if (it == m_Assets.end()) return texPtr;
        if (!it->second.texture) it->second.texture = texPtr;
        return it->second.texture;
    }
    return nullptr;
}

std::shared_ptr<Mesh> AssetLibrary::CreatePrimitiveMesh(const std::string& primitiveType) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    // Check if we already have this primitive cached
    auto it = m_PrimitiveMeshes.find(primitiveType);
    if (it != m_PrimitiveMeshes.end()) {
        return it->second;
    }
    
    // Create the primitive mesh
    std::shared_ptr<Mesh> mesh;
    if (primitiveType == "Cube") {
        mesh = StandardMeshManager::Instance().GetCubeMesh();
    } else if (primitiveType == "Sphere") {
        mesh = StandardMeshManager::Instance().GetSphereMesh();
    } else if (primitiveType == "Plane") {
        mesh = StandardMeshManager::Instance().GetPlaneMesh();
    } else if (primitiveType == "Capsule") {
        mesh = StandardMeshManager::Instance().GetCapsuleMesh();
    } else {
        std::cout << "[AssetLibrary] Warning: Unknown primitive type: " << primitiveType << std::endl;
        mesh = StandardMeshManager::Instance().GetCubeMesh(); // Fallback to cube
    }
    
    // Cache the primitive mesh
    m_PrimitiveMeshes[primitiveType] = mesh;
    return mesh;
}

ClaymoreGUID AssetLibrary::GetGUIDForPath(const std::string& path) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    // Normalize slashes
    std::string key = path;
    std::replace(key.begin(), key.end(), '\\', '/');
    // Direct lookup
    auto it = m_PathToGUID.find(key);
    if (it != m_PathToGUID.end()) return it->second;
    // If absolute under project, convert to project-relative
    try {
        std::filesystem::path proj = Project::GetProjectDirectory();
        if (!proj.empty()) {
            std::error_code ec;
            std::filesystem::path rel = std::filesystem::relative(key, proj, ec);
            if (!ec) {
                std::string v = rel.string();
                std::replace(v.begin(), v.end(), '\\', '/');
                auto it2 = m_PathToGUID.find(v);
                if (it2 != m_PathToGUID.end()) return it2->second;
            }
        }
    } catch(...) {}
    // If the string contains assets/, use substring from there
    size_t pos = key.find("assets/");
    if (pos != std::string::npos) {
        std::string v = key.substr(pos);
        auto it3 = m_PathToGUID.find(v);
        if (it3 != m_PathToGUID.end()) return it3->second;
    }
    return ClaymoreGUID();
}

std::string AssetLibrary::GetPathForGUID(const ClaymoreGUID& guid) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    auto it = m_GUIDToPath.find(guid);
    return it != m_GUIDToPath.end() ? it->second : "";
}

std::vector<std::tuple<std::string, ClaymoreGUID, AssetType>> AssetLibrary::GetAllAssets() const {
    std::vector<std::tuple<std::string, ClaymoreGUID, AssetType>> out;
    out.reserve(m_Assets.size());
    for (const auto& [guid, entry] : m_Assets) {
        out.emplace_back(entry.path, guid, entry.type);
    }
    return out;
}

void AssetLibrary::Clear() {
    std::lock_guard<std::mutex> lk(m_Mutex);
    m_Assets.clear();
    m_PathToGUID.clear();
    m_GUIDToPath.clear();
    m_PrimitiveMeshes.clear();
}

void AssetLibrary::PrintAllAssets() const {
    std::cout << "[AssetLibrary] Registered Assets:" << std::endl;
    std::lock_guard<std::mutex> lk(m_Mutex);
    for (const auto& pair : m_Assets) {
        const AssetEntry& entry = pair.second;
        std::cout << "  - " << entry.name << " (GUID: " << entry.reference.guid.ToString() 
                  << ", Path: " << entry.path << ")" << std::endl;
    }
} 

// ----------------------------------------------
// Fast-path helpers for .meta/.meshbin
// ----------------------------------------------
std::string AssetLibrary::ResolveMeshBinFromMeta(const std::string& metaPath, int /*fileID*/) {
    try {
        std::ifstream in(metaPath);
        if (!in.is_open()) return {};
        nlohmann::json j; in >> j; in.close();
        if (j.contains("meshes") && j["meshes"].is_array() && !j["meshes"].empty()) {
            // Read first mesh entry's "mesh" string and strip optional #index
            std::string m = j["meshes"][0].value("mesh", std::string());
            if (m.empty()) return {};
            // If string contains #, split
            auto pos = m.find('#');
            if (pos != std::string::npos) m = m.substr(0, pos);
            return m;
        }
    } catch (...) {}
    return {};
}

std::shared_ptr<Mesh> AssetLibrary::LoadMeshBin(const std::string& meshBinPath, int /*fileID*/) {
    // Stub: allocate an empty mesh for now; real implementation will map/read streams and create GPU buffers
    auto mesh = std::make_shared<Mesh>();
    mesh->numVertices = 0;
    mesh->numIndices = 0;
    return mesh;
}