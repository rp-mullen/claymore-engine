#include "AssetLibrary.h"
#include "AssetRegistry.h"
#include "rendering/StandardMeshManager.h"
#include "rendering/MaterialManager.h"
#include "rendering/ModelLoader.h"
#include "rendering/TextureLoader.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

void AssetLibrary::RegisterAsset(const AssetReference& ref, AssetType type, const std::string& path, const std::string& name) {
    AssetEntry entry(ref, type, path, name);
    m_Assets[ref.guid] = entry;
    m_PathToGUID[path] = ref.guid;
    m_GUIDToPath[ref.guid] = path;
    
    std::cout << "[AssetLibrary] Registered asset: " << name << " (GUID: " << ref.guid.ToString() << ")" << std::endl;
}

void AssetLibrary::UnregisterAsset(const AssetReference& ref) {
    auto it = m_Assets.find(ref.guid);
    if (it != m_Assets.end()) {
        m_PathToGUID.erase(it->second.path);
        m_GUIDToPath.erase(ref.guid);
        m_Assets.erase(it);
    }
}

AssetEntry* AssetLibrary::GetAsset(const AssetReference& ref) {
    auto it = m_Assets.find(ref.guid);
    return it != m_Assets.end() ? &it->second : nullptr;
}

AssetEntry* AssetLibrary::GetAsset(const ClaymoreGUID& guid) {
    auto it = m_Assets.find(guid);
    return it != m_Assets.end() ? &it->second : nullptr;
}

AssetEntry* AssetLibrary::GetAsset(const std::string& path) {
    auto it = m_PathToGUID.find(path);
    if (it != m_PathToGUID.end()) {
        return GetAsset(it->second);
    }
    return nullptr;
}

std::shared_ptr<Mesh> AssetLibrary::LoadMesh(const AssetReference& ref) {
    AssetEntry* entry = GetAsset(ref);
    if (!entry) {
        std::cout << "[AssetLibrary] Warning: Asset not found for GUID: " << ref.guid.ToString() << std::endl;
        return nullptr;
    }
    
    // If mesh is already loaded, return it
    if (entry->mesh) {
        return entry->mesh;
    }
    
    // Check if it's a primitive
    if (ref.guid == AssetReference::CreatePrimitive("").guid) {
        // This is a primitive mesh, create it
        std::string primitiveType = entry->name;
        entry->mesh = CreatePrimitiveMesh(primitiveType);
        return entry->mesh;
    }
    
    // Load mesh from file
    if (!entry->path.empty()) {
        Model model = ModelLoader::LoadModel(entry->path);
        if (!model.Meshes.empty()) {
            entry->mesh = model.Meshes[0]; // Use the first mesh from the model
            std::cout << "[AssetLibrary] Loaded mesh: " << entry->name << std::endl;
        } else {
            std::cout << "[AssetLibrary] Failed to load mesh: " << entry->path << std::endl;
        }
        return entry->mesh;
    }
    
    return nullptr;
}

std::shared_ptr<Material> AssetLibrary::LoadMaterial(const AssetReference& ref) {
    AssetEntry* entry = GetAsset(ref);
    if (!entry) {
        return nullptr;
    }
    
    // If material is already loaded, return it
    if (entry->material) {
        return entry->material;
    }
    
    // For now, create a default PBR material
    entry->material = MaterialManager::Instance().CreateDefaultPBRMaterial();
    return entry->material;
}

std::shared_ptr<bgfx::TextureHandle> AssetLibrary::LoadTexture(const AssetReference& ref) {
    AssetEntry* entry = GetAsset(ref);
    if (!entry) {
        return nullptr;
    }
    
    // If texture is already loaded, return it
    if (entry->texture) {
        return entry->texture;
    }
    
    // Load texture from file
    if (!entry->path.empty()) {
        bgfx::TextureHandle handle = TextureLoader::Load2D(entry->path);
        entry->texture = std::make_shared<bgfx::TextureHandle>(handle);
        return entry->texture;
    }
    
    return nullptr;
}

std::shared_ptr<Mesh> AssetLibrary::CreatePrimitiveMesh(const std::string& primitiveType) {
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
    } else {
        std::cout << "[AssetLibrary] Warning: Unknown primitive type: " << primitiveType << std::endl;
        mesh = StandardMeshManager::Instance().GetCubeMesh(); // Fallback to cube
    }
    
    // Cache the primitive mesh
    m_PrimitiveMeshes[primitiveType] = mesh;
    return mesh;
}

ClaymoreGUID AssetLibrary::GetGUIDForPath(const std::string& path) {
    auto it = m_PathToGUID.find(path);
    return it != m_PathToGUID.end() ? it->second : ClaymoreGUID();
}

std::string AssetLibrary::GetPathForGUID(const ClaymoreGUID& guid) {
    auto it = m_GUIDToPath.find(guid);
    return it != m_GUIDToPath.end() ? it->second : "";
}

void AssetLibrary::Clear() {
    m_Assets.clear();
    m_PathToGUID.clear();
    m_GUIDToPath.clear();
    m_PrimitiveMeshes.clear();
}

void AssetLibrary::PrintAllAssets() const {
    std::cout << "[AssetLibrary] Registered Assets:" << std::endl;
    for (const auto& pair : m_Assets) {
        const AssetEntry& entry = pair.second;
        std::cout << "  - " << entry.name << " (GUID: " << entry.reference.guid.ToString() 
                  << ", Path: " << entry.path << ")" << std::endl;
    }
} 