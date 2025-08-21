#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include "AssetReference.h"
#include "rendering/Mesh.h"
#include "rendering/Material.h"
#include "rendering/TextureLoader.h"
#include "animation/AnimationTypes.h"
#include <mutex>

// Forward declarations to avoid heavy includes
struct EntityData;
class Scene;

// Asset types enum
enum class AssetType {
    Mesh = 3,
    Texture = 2,
    Material = 21,
    Shader = 48,
    Script = 115,
    Animation = 196,
    Prefab = 250
};

// Asset entry in the library
struct AssetEntry {
    AssetReference reference;
    AssetType type;
    std::string path;
    std::string name;
    
    // Runtime data
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    std::shared_ptr<bgfx::TextureHandle> texture;
    std::shared_ptr<cm::animation::AnimationClip> animation;
    
    AssetEntry() = default;
    AssetEntry(const AssetReference& ref, AssetType t, const std::string& p, const std::string& n)
        : reference(ref), type(t), path(p), name(n) {}
};

class AssetLibrary {
public:
    static AssetLibrary& Instance() {
        static AssetLibrary instance;
        return instance;
    }
    
    // Asset registration
    void RegisterAsset(const AssetReference& ref, AssetType type, const std::string& path, const std::string& name);
    // Register an alternate path string (absolute or virtual) that should also resolve to the same GUID
    void RegisterPathAlias(const ClaymoreGUID& guid, const std::string& altPath);
    void UnregisterAsset(const AssetReference& ref);
    
                // Asset lookup
            AssetEntry* GetAsset(const AssetReference& ref);
            AssetEntry* GetAsset(const ClaymoreGUID& guid);
            AssetEntry* GetAsset(const std::string& path);
    
    // Asset loading
    std::shared_ptr<Mesh> LoadMesh(const AssetReference& ref);
    std::shared_ptr<Material> LoadMaterial(const AssetReference& ref);
    std::shared_ptr<bgfx::TextureHandle> LoadTexture(const AssetReference& ref);
    bool LoadPrefabIntoEntity(const AssetReference& ref, EntityData& outEntity, Scene& scene);
    
    // Primitive mesh creation
    std::shared_ptr<Mesh> CreatePrimitiveMesh(const std::string& primitiveType);
    
                // Asset path to GUID mapping
            ClaymoreGUID GetGUIDForPath(const std::string& path);
            std::string GetPathForGUID(const ClaymoreGUID& guid);
            std::vector<std::tuple<std::string, ClaymoreGUID, AssetType>> GetAllAssets() const;
    
    // Clear all assets
    void Clear();
    
    // Debug
    void PrintAllAssets() const;
    
    // Internal helpers for fast-path model cache loading
    static std::string ResolveMeshBinFromMeta(const std::string& metaPath, int fileID);
    static std::shared_ptr<Mesh> LoadMeshBin(const std::string& meshBinPath, int fileID);

private:
    AssetLibrary() = default;
    ~AssetLibrary() = default;
    
                mutable std::mutex m_Mutex;
                std::unordered_map<ClaymoreGUID, AssetEntry> m_Assets;
            std::unordered_map<std::string, ClaymoreGUID> m_PathToGUID;
            std::unordered_map<ClaymoreGUID, std::string> m_GUIDToPath;
    
    // Primitive mesh cache
    std::unordered_map<std::string, std::shared_ptr<Mesh>> m_PrimitiveMeshes;
}; 