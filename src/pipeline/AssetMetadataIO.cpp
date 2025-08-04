#include <nlohmann/json.hpp>
#include "AssetMetadataIO.h"
#include <fstream>

bool AssetMetadataIO::Load(const std::string& metaPath, AssetMetadata& outMeta) {
    if (!std::filesystem::exists(metaPath)) return false;
    std::ifstream in(metaPath);
    nlohmann::json j;
    in >> j;
    outMeta.type = j["type"];
    outMeta.hash = j["hash"];
    outMeta.lastImported = j["lastImported"];
    for (auto& [key, val] : j["settings"].items())
        outMeta.settings[key] = val;
    return true;
}

void AssetMetadataIO::Save(const std::string& metaPath, const AssetMetadata& meta) {
    nlohmann::json j;
    j["type"] = meta.type;
    j["hash"] = meta.hash;
    j["lastImported"] = meta.lastImported;
    j["settings"] = meta.settings;
    std::ofstream out(metaPath);
    out << j.dump(4);
}
