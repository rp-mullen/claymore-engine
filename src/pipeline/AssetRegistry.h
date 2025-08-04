#pragma once
#include <unordered_map>
#include <string>
#include "AssetMetadata.h"

class AssetRegistry {
public:
    static AssetRegistry& Instance() {
        static AssetRegistry instance;
        return instance;
    }

    const AssetMetadata* GetMetadata(const std::string& path) const;
    void SetMetadata(const std::string& path, const AssetMetadata& meta);
    bool HasMetadata(const std::string& path) const;
    void RemoveMetadata(const std::string& path);
    void Clear();
    void SaveToDisk(const std::string& file);
    void LoadFromDisk(const std::string& file);
    void PrintAll() const;

private:
    AssetRegistry() = default;
    std::unordered_map<std::string, AssetMetadata> m_Metadata;
};
