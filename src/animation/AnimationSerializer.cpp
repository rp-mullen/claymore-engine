#include "animation/AnimationSerializer.h"
#include <fstream>

namespace cm {
namespace animation {

// ---------------- Keyframes ------------------
json SerializeKeyframe(const KeyframeVec3& kf) {
    return json{{"t", kf.Time}, {"v", {kf.Value.x, kf.Value.y, kf.Value.z}}};
}

json SerializeKeyframe(const KeyframeQuat& kf) {
    return json{{"t", kf.Time}, {"v", {kf.Value.x, kf.Value.y, kf.Value.z, kf.Value.w}}};
}

json SerializeKeyframe(const KeyframeFloat& kf) {
    return json{{"t", kf.Time}, {"v", kf.Value}};
}

KeyframeVec3 DeserializeKeyframeVec3(const json& j) {
    KeyframeVec3 kf;
    kf.Time = j["t"].get<float>();
    const auto& arr = j["v"];
    kf.Value = glm::vec3(arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>());
    return kf;
}

KeyframeQuat DeserializeKeyframeQuat(const json& j) {
    KeyframeQuat kf;
    kf.Time = j["t"].get<float>();
    const auto& arr = j["v"];
    kf.Value = glm::quat(arr[3].get<float>(), arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>());
    return kf;
}

KeyframeFloat DeserializeKeyframeFloat(const json& j) {
    KeyframeFloat kf;
    kf.Time = j["t"].get<float>();
    kf.Value = j["v"].get<float>();
    return kf;
}

// --------------- Clip --------------------
json SerializeAnimationClip(const AnimationClip& clip) {
    json j;
    j["name"] = clip.Name;
    j["duration"] = clip.Duration;
    j["tps"] = clip.TicksPerSecond;

    json tracksJson;
    for (const auto& [boneName, track] : clip.BoneTracks) {
        json t;
        for (const auto& k : track.PositionKeys) t["pos"].push_back(SerializeKeyframe(k));
        for (const auto& k : track.RotationKeys) t["rot"].push_back(SerializeKeyframe(k));
        for (const auto& k : track.ScaleKeys)    t["scl"].push_back(SerializeKeyframe(k));
        tracksJson[boneName] = std::move(t);
    }
    j["tracks"] = std::move(tracksJson);
    return j;
}

AnimationClip DeserializeAnimationClip(const json& j) {
    AnimationClip clip;
    clip.Name = j.value("name", "");
    clip.Duration = j.value("duration", 0.0f);
    clip.TicksPerSecond = j.value("tps", 0.0f);

    if (j.contains("tracks")) {
        for (auto it = j["tracks"].begin(); it != j["tracks"].end(); ++it) {
            BoneTrack track;
            const json& t = it.value();
            if (t.contains("pos")) {
                for (const auto& k : t["pos"]) track.PositionKeys.push_back(DeserializeKeyframeVec3(k));
            }
            if (t.contains("rot")) {
                for (const auto& k : t["rot"]) track.RotationKeys.push_back(DeserializeKeyframeQuat(k));
            }
            if (t.contains("scl")) {
                for (const auto& k : t["scl"]) track.ScaleKeys.push_back(DeserializeKeyframeVec3(k));
            }
            clip.BoneTracks[it.key()] = std::move(track);
        }
    }
    return clip;
}

bool SaveAnimationClip(const AnimationClip& clip, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << SerializeAnimationClip(clip).dump(4);
    return true;
}

AnimationClip LoadAnimationClip(const std::string& path) {
    std::ifstream file(path);
    json j; file >> j;
    return DeserializeAnimationClip(j);
}

} // namespace animation
} // namespace cm
