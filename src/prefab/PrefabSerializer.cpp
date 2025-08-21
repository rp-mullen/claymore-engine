#include "prefab/PrefabSerializer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace PrefabIO {

static void to_json(json& j, const PrefabAssetEntityNode& n) {
    j = json{};
    j["guid"] = n.Guid;
    j["name"] = n.Name;
    if (n.ParentGuid.high != 0 || n.ParentGuid.low != 0) j["parent"] = n.ParentGuid;
    j["components"] = n.Components;
    j["children"] = json::array();
    for (auto& c : n.Children) j["children"].push_back(c);
}

static void from_json(const json& j, PrefabAssetEntityNode& n) {
    j.at("guid").get_to(n.Guid);
    n.Name = j.value("name", std::string());
    try { if (j.contains("parent")) j.at("parent").get_to(n.ParentGuid); } catch(...) {}
    if (j.contains("components")) n.Components = j["components"]; else n.Components = json::object();
    n.Children.clear();
    if (j.contains("children") && j["children"].is_array()) {
        for (const auto& c : j["children"]) n.Children.push_back(c.get<ClaymoreGUID>());
    }
}

bool LoadAuthoringPrefabJSON(const std::string& path, PrefabAsset& out) {
    std::ifstream in(path);
    if (!in.is_open()) { std::cerr << "[PrefabIO] Cannot open authoring prefab: " << path << std::endl; return false; }
    json j; in >> j; in.close();
    try {
        j.at("guid").get_to(out.Guid);
        out.Name = j.value("name", std::string());
        j.at("root").get_to(out.RootGuid);
        out.Entities.clear();
        if (j.contains("entities")) {
            for (const auto& e : j["entities"]) {
                PrefabAssetEntityNode node; from_json(e, node); out.Entities.push_back(std::move(node));
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[PrefabIO] LoadAuthoringPrefabJSON error: " << e.what() << std::endl; return false;
    }
}

bool SaveAuthoringPrefabJSON(const std::string& path, const PrefabAsset& in) {
    json j;
    j["guid"] = in.Guid;
    j["name"] = in.Name;
    j["root"] = in.RootGuid;
    j["entities"] = json::array();
    for (const auto& e : in.Entities) { json je; to_json(je, e); j["entities"].push_back(std::move(je)); }
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) { std::cerr << "[PrefabIO] Cannot write authoring prefab: " << path << std::endl; return false; }
    out << j.dump(4);
    return true;
}

bool LoadVariantJSON(const std::string& path, ClaymoreGUID& outGuid, ClaymoreGUID& outBaseGuid, PrefabOverrides& outOv) {
    std::ifstream in(path);
    if (!in.is_open()) { std::cerr << "[PrefabIO] Cannot open variant: " << path << std::endl; return false; }
    json j; in >> j; in.close();
    try {
        j.at("guid").get_to(outGuid);
        j.at("basePrefab").get_to(outBaseGuid);
        outOv.Ops.clear();
        if (j.contains("overrides") && j["overrides"].is_array()) {
            for (const auto& o : j["overrides"]) {
                OverrideOp op;
                op.Op = o.value("op", std::string());
                op.Path = o.value("path", std::string());
                if (o.contains("value")) op.Value = o["value"]; else op.Value = json();
                outOv.Ops.push_back(std::move(op));
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[PrefabIO] LoadVariantJSON error: " << e.what() << std::endl; return false;
    }
}

bool SaveVariantJSON(const std::string& path, const ClaymoreGUID& guid, const ClaymoreGUID& baseGuid, const PrefabOverrides& ov) {
    json j;
    j["guid"] = guid;
    j["basePrefab"] = baseGuid;
    j["overrides"] = json::array();
    for (const auto& op : ov.Ops) {
        json o; o["op"] = op.Op; o["path"] = op.Path; if (!op.Value.is_null()) o["value"] = op.Value; j["overrides"].push_back(std::move(o));
    }
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) { std::cerr << "[PrefabIO] Cannot write variant: " << path << std::endl; return false; }
    out << j.dump(4);
    return true;
}

}


