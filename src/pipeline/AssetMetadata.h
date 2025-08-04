#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "AssetReference.h"

struct AssetMetadata {
    std::string sourcePath;    // Original file location
    std::string processedPath; // Engine-ready version (e.g., cache/)
    std::string type;          // "model", "texture", "shader"
    std::string hash;          // Hash of file contents (or timestamp)
    std::string lastImported;  // ISO 8601 timestamp (optional)
    std::unordered_map<std::string, std::string> settings; // Pipeline options
    ClaymoreGUID guid;                 // Unique identifier for this asset
    AssetReference reference;   // Asset reference for serialization
};

inline void to_json(nlohmann::json& j, const AssetMetadata& meta) {
    j = {
        {"sourcePath", meta.sourcePath},
        {"processedPath", meta.processedPath},
        {"type", meta.type},
        {"hash", meta.hash},
        {"lastImported", meta.lastImported},
        {"settings", meta.settings},
        {"guid", meta.guid},
        {"reference", meta.reference}
    };
}

inline void from_json(const nlohmann::json& j, AssetMetadata& meta) {
    j.at("sourcePath").get_to(meta.sourcePath);
    j.at("processedPath").get_to(meta.processedPath);
    j.at("type").get_to(meta.type);
    j.at("hash").get_to(meta.hash);
    j.at("lastImported").get_to(meta.lastImported);
    j.at("settings").get_to(meta.settings);
    if (j.contains("guid")) j.at("guid").get_to(meta.guid);
    if (j.contains("reference")) j.at("reference").get_to(meta.reference);
}