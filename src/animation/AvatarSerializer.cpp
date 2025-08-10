#include "animation/AvatarSerializer.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace cm {
namespace animation {

static void to_json(json& j, const AvatarDefinition& a)
{
    j["rig"] = a.RigName;
    j["unitsPerMeter"] = a.UnitsPerMeter;
    j["axes"] = {
        {"up", (int)a.Axes.Up},
        {"forward", (int)a.Axes.Forward},
        {"rightHanded", a.Axes.RightHanded}
    };
    // Map
    json map = json::array();
    for (uint16_t i = 0; i < HumanoidBoneCount; ++i) {
        const auto& e = a.Map[i];
        json entry;
        entry["bone"] = (int)i;
        entry["name"] = e.BoneName;
        entry["index"] = e.BoneIndex;
        entry["present"] = a.Present[i];
        map.push_back(entry);
    }
    j["map"] = std::move(map);
}

static void from_json(const json& j, AvatarDefinition& a)
{
    a = AvatarDefinition{}; // reset sizes
    a.RigName = j.value("rig", "");
    a.UnitsPerMeter = j.value("unitsPerMeter", 1.0f);
    if (j.contains("axes")) {
        a.Axes.Up = (AvatarAxes::Axis)j["axes"].value("up", (int)AvatarAxes::Axis::Y);
        a.Axes.Forward = (AvatarAxes::Axis)j["axes"].value("forward", (int)AvatarAxes::Axis::Z);
        a.Axes.RightHanded = j["axes"].value("rightHanded", true);
    }
    if (j.contains("map") && j["map"].is_array()) {
        for (const auto& entry : j["map"]) {
            uint16_t bi = (uint16_t)entry.value("bone", 0);
            if (bi >= HumanoidBoneCount) continue;
            a.Map[bi].Bone = (HumanoidBone)bi;
            a.Map[bi].BoneName = entry.value("name", "");
            a.Map[bi].BoneIndex = entry.value("index", -1);
            a.Present[bi] = entry.value("present", false);
        }
    }
}

bool SaveAvatar(const AvatarDefinition& avatar, const std::string& path)
{
    json j; to_json(j, avatar);
    std::ofstream out(path);
    if (!out) return false;
    out << j.dump(4);
    return true;
}

bool LoadAvatar(AvatarDefinition& avatar, const std::string& path)
{
    std::ifstream in(path);
    if (!in) return false;
    json j; in >> j;
    from_json(j, avatar);
    return true;
}

} // namespace animation
} // namespace cm


