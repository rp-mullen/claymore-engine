#include "prefab/PrefabCache.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

static std::string PrefabCachePath(const ClaymoreGUID& guid) {
    // For now, write next to project assets using GUID-based filename
    return std::string("assets/prefabs/") + guid.ToString() + ".prefabcb";
}

bool LoadCompiledPrefab(const ClaymoreGUID& prefabGuid, CompiledPrefab& out) {
    std::string path = PrefabCachePath(prefabGuid);
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    try {
        json j; in >> j; in.close();
        out.PrefabGuid = prefabGuid;
        out.EngineVersion = j.value("engineVersion", std::string());
        out.PrefabHash = j.value("prefabHash", 0ull);
        out.Entities.clear();
        if (j.contains("entities")) {
            for (const auto& e : j["entities"]) {
                CompiledPrefabEntityRecord rec;
                e.at("guid").get_to(rec.EntityGuid);
                rec.Name = e.value("name", std::string());
                if (e.contains("components")) rec.Components = e["components"]; else rec.Components = json::object();
                if (e.contains("skinned")) {
                    const json& s = e["skinned"];
                    rec.Skinned.PaletteSize = s.value("paletteSize", 0u);
                    if (s.contains("remap")) rec.Skinned.Remap = s["remap"].get<std::vector<uint16_t>>();
                    if (s.contains("used")) rec.Skinned.UsedJointList = s["used"].get<std::vector<uint16_t>>();
                }
                out.Entities.push_back(std::move(rec));
            }
        }
        out.ReferencedAssetImportHashes.clear();
        if (j.contains("assetHashes")) {
            for (const auto& a : j["assetHashes"]) {
                ClaymoreGUID g; std::string h; a.at("guid").get_to(g); h = a.value("hash", std::string());
                out.ReferencedAssetImportHashes.emplace_back(g, std::move(h));
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool WriteCompiledPrefab(const ClaymoreGUID& prefabGuid, const CompiledPrefab& in) {
    std::string path = PrefabCachePath(prefabGuid);
    std::error_code ec; std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    json j;
    j["engineVersion"] = in.EngineVersion;
    j["prefabHash"] = in.PrefabHash;
    j["entities"] = json::array();
    for (const auto& e : in.Entities) {
        json je; je["guid"] = e.EntityGuid; je["name"] = e.Name; je["components"] = e.Components;
        json sk; sk["paletteSize"] = e.Skinned.PaletteSize; sk["remap"] = e.Skinned.Remap; sk["used"] = e.Skinned.UsedJointList;
        je["skinned"] = std::move(sk);
        j["entities"].push_back(std::move(je));
    }
    j["assetHashes"] = json::array();
    for (auto& p : in.ReferencedAssetImportHashes) { json e; e["guid"] = p.first; e["hash"] = p.second; j["assetHashes"].push_back(std::move(e)); }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) { std::cerr << "[PrefabCache] Cannot write: " << path << std::endl; return false; }
    out << j.dump(2);
    return true;
}


