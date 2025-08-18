#include "MaterialImporter.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace MaterialImporter {

static void to_json(json& j, const MaterialAssetUnified& m) {
    j = json{
        {"name", m.name},
        {"shader", m.shaderPath}
    };
    json jp = json::object();
    for (const auto& kv : m.params) {
        jp[kv.first] = { kv.second.x, kv.second.y, kv.second.z, kv.second.w };
    }
    j["params"] = std::move(jp);
    j["textures"] = m.textures;
}

static void from_json(const json& j, MaterialAssetUnified& m) {
    if (j.contains("name")) m.name = j.at("name").get<std::string>();
    if (j.contains("shader")) m.shaderPath = j.at("shader").get<std::string>();
    if (j.contains("params") && j["params"].is_object()) {
        for (auto it = j["params"].begin(); it != j["params"].end(); ++it) {
            const auto& arr = it.value();
            if (arr.is_array() && arr.size() == 4) {
                m.params[it.key()] = { arr[0], arr[1], arr[2], arr[3] };
            }
        }
    }
    if (j.contains("textures") && j["textures"].is_object()) {
        for (auto it = j["textures"].begin(); it != j["textures"].end(); ++it) {
            m.textures[it.key()] = it.value().get<std::string>();
        }
    }
}

bool Load(const std::string& path, MaterialAssetUnified& out) {
    std::ifstream in(path);
    if (!in) return false;
    json j; in >> j; in.close();
    try { from_json(j, out); } catch (...) { return false; }
    return true;
}

bool Save(const std::string& path, const MaterialAssetUnified& in) {
    std::ofstream outF(path);
    if (!outF) return false;
    json j; to_json(j, in);
    outF << j.dump(4);
    return true;
}

} // namespace MaterialImporter


