#include "AssetRegistry.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

// ----------------------------
// Get Metadata for a given path
// ----------------------------
const AssetMetadata* AssetRegistry::GetMetadata(const std::string& path) const {
    std::lock_guard<std::mutex> lk(m_Mutex);
    auto it = m_Metadata.find(path);
    return (it != m_Metadata.end()) ? &it->second : nullptr;
}

// ----------------------------
// Set Metadata for a given path
// ----------------------------
void AssetRegistry::SetMetadata(const std::string& path, const AssetMetadata& meta) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    m_Metadata[path] = meta;
}

// ----------------------------
// Check if a file has metadata
// ----------------------------
bool AssetRegistry::HasMetadata(const std::string& path) const {
    std::lock_guard<std::mutex> lk(m_Mutex);
    return m_Metadata.find(path) != m_Metadata.end();
}

// ----------------------------
// Remove metadata entry
// ----------------------------
void AssetRegistry::RemoveMetadata(const std::string& path) {
    std::lock_guard<std::mutex> lk(m_Mutex);
    m_Metadata.erase(path);
}

// ----------------------------
// Clear all metadata (used for project reset)
// ----------------------------
void AssetRegistry::Clear() {
    std::lock_guard<std::mutex> lk(m_Mutex);
    m_Metadata.clear();
}

// ----------------------------
// Save all metadata to disk as JSON
// ----------------------------
void AssetRegistry::SaveToDisk(const std::string& file) {
    json j;
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        for (const auto& [path, meta] : m_Metadata) {
            json metaJson;
            metaJson["sourcePath"] = meta.sourcePath;
            metaJson["processedPath"] = meta.processedPath;
            metaJson["type"] = meta.type;
            metaJson["hash"] = meta.hash;
            metaJson["lastImported"] = meta.lastImported;
            metaJson["settings"] = meta.settings; // unordered_map will serialize as object

            j[path] = metaJson;
        }
    }

    std::ofstream out(file);
    if (out.is_open()) {
        out << j.dump(4);
        std::cout << "[AssetRegistry] Saved metadata to: " << file << std::endl;
    }
    else {
        std::cerr << "[AssetRegistry] Failed to save metadata to: " << file << std::endl;
    }
}

// ----------------------------
// Load metadata from disk
// ----------------------------
void AssetRegistry::LoadFromDisk(const std::string& file) {
    std::ifstream in(file);
    if (!in.is_open()) {
        std::cerr << "[AssetRegistry] Metadata file not found: " << file << std::endl;
        return;
    }

    json j;
    in >> j;

    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        m_Metadata.clear();
        for (auto& [path, metaJson] : j.items()) {
        AssetMetadata meta;
        meta.sourcePath = metaJson.value("sourcePath", "");
        meta.processedPath = metaJson.value("processedPath", "");
        meta.type = metaJson.value("type", "");
        meta.hash = metaJson.value("hash", "");
        meta.lastImported = metaJson.value("lastImported", "");

        if (metaJson.contains("settings") && metaJson["settings"].is_object()) {
            meta.settings = metaJson["settings"].get<std::unordered_map<std::string, std::string>>();
        }

        m_Metadata[path] = meta;
        }
    }

    std::cout << "[AssetRegistry] Loaded metadata from: " << file << std::endl;
}

// ----------------------------
// Print all registry entries (debugging)
// ----------------------------
void AssetRegistry::PrintAll() const {
    std::cout << "\n--- Asset Registry ---\n";
    for (const auto& [path, meta] : m_Metadata) {
        std::cout << "Asset: " << path << "\n"
            << "  Type: " << meta.type << "\n"
            << "  Hash: " << meta.hash << "\n"
            << "  Last Imported: " << meta.lastImported << "\n"
            << "  Processed Path: " << meta.processedPath << "\n\n";
    }
}
