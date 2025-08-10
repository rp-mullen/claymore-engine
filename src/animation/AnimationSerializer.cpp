#include "animation/AnimationSerializer.h"
#include <fstream>
#include "animation/PropertyTrack.h"
#include <iostream>
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
    j["humanoid"] = clip.IsHumanoid;
    if (!clip.SourceAvatarRigName.empty()) j["avatarRig"] = clip.SourceAvatarRigName;
    if (!clip.SourceAvatarPath.empty()) j["avatarPath"] = clip.SourceAvatarPath;
    if (clip.IsHumanoid && !clip.HumanoidTracks.empty()) {
        json h;
        for (const auto& [id, bt] : clip.HumanoidTracks) {
            json t;
            if (!bt.PositionKeys.empty()) { json arr = json::array(); for (const auto& k : bt.PositionKeys) arr.push_back(SerializeKeyframe(k)); t["pos"] = std::move(arr); }
            if (!bt.RotationKeys.empty()) { json arr = json::array(); for (const auto& k : bt.RotationKeys) arr.push_back(SerializeKeyframe(k)); t["rot"] = std::move(arr); }
            if (!bt.ScaleKeys.empty())    { json arr = json::array(); for (const auto& k : bt.ScaleKeys) arr.push_back(SerializeKeyframe(k)); t["scl"] = std::move(arr); }
            h[std::to_string(id)] = std::move(t);
        }
        j["humanoidTracks"] = std::move(h);
    }
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
    clip.IsHumanoid = j.value("humanoid", false);
    clip.SourceAvatarRigName = j.value("avatarRig", "");
    clip.SourceAvatarPath = j.value("avatarPath", "");
    if (clip.IsHumanoid && j.contains("humanoidTracks")) {
        for (auto it = j["humanoidTracks"].begin(); it != j["humanoidTracks"].end(); ++it) {
            BoneTrack track; const json& t = it.value();
            if (t.contains("pos")) for (const auto& k : t["pos"]) track.PositionKeys.push_back(DeserializeKeyframeVec3(k));
            if (t.contains("rot")) for (const auto& k : t["rot"]) track.RotationKeys.push_back(DeserializeKeyframeQuat(k));
            if (t.contains("scl")) for (const auto& k : t["scl"]) track.ScaleKeys.push_back(DeserializeKeyframeVec3(k));
            int id = std::stoi(it.key());
            clip.HumanoidTracks[id] = std::move(track);
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
    AnimationClip empty{};
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[AnimationSerializer] Failed to open animation clip: " << path << "\n";
            return empty;
        }
        json j;
        file >> j;
        return DeserializeAnimationClip(j);
    } catch (const std::exception& e) {
        std::cerr << "[AnimationSerializer] Error loading animation clip '" << path << "': " << e.what() << "\n";
        return empty;
    }
}

// ---------------- Timeline Clip (property + script) ----------------
namespace {
    json SerializePropertyTrack(const PropertyTrack& t)
    {
        json j;
        j["path"] = t.PropertyPath;
        j["keys"] = json::array();
        for (const auto& k : t.Keys) j["keys"].push_back(SerializeKeyframe(k));
        return j;
    }

    PropertyTrack DeserializePropertyTrack(const json& j)
    {
        PropertyTrack t;
        t.PropertyPath = j.value("path", "");
        if (j.contains("keys"))
            for (const auto& k : j["keys"]) t.Keys.push_back(DeserializeKeyframeFloat(k));
        return t;
    }

    json SerializeScriptTrack(const ScriptEventTrack& t)
    {
        json j;
        j["name"] = t.Name;
        j["keys"] = json::array();
        for (const auto& k : t.Keys)
            j["keys"].push_back({{"t", k.Time}, {"class", k.ScriptClass}, {"method", k.Method}});
        return j;
    }

    ScriptEventTrack DeserializeScriptTrack(const json& j)
    {
        ScriptEventTrack t;
        t.Name = j.value("name", "Script Events");
        if (j.contains("keys"))
            for (const auto& kj : j["keys"]) {
                ScriptEventKey k;
                k.Time = kj.value("t", 0.0f);
                k.ScriptClass = kj.value("class", "");
                k.Method = kj.value("method", "");
                t.Keys.push_back(std::move(k));
            }
        return t;
    }
}

json SerializeTimelineClip(const TimelineClip& clip)
{
    json j;
    j["name"] = clip.Name;
    j["length"] = clip.Length;
    j["tracks"] = json::array();
    for (const auto& t : clip.Tracks) j["tracks"].push_back(SerializePropertyTrack(t));
    j["scriptTracks"] = json::array();
    for (const auto& t : clip.ScriptTracks) j["scriptTracks"].push_back(SerializeScriptTrack(t));
    // optional skeletal clips
    if (!clip.SkeletalClips.empty()) {
        json arr = json::array();
        for (const auto& sc : clip.SkeletalClips) arr.push_back({{"path", sc.ClipPath}, {"speed", sc.Speed}, {"loop", sc.Loop}});
        j["skeletal"] = std::move(arr);
    }
    return j;
}

TimelineClip DeserializeTimelineClip(const json& j)
{
    TimelineClip clip;
    clip.Name = j.value("name", "");
    clip.Length = j.value("length", 0.0f);
    if (j.contains("tracks"))
        for (const auto& tj : j["tracks"]) clip.Tracks.push_back(DeserializePropertyTrack(tj));
    if (j.contains("scriptTracks"))
        for (const auto& sj : j["scriptTracks"]) clip.ScriptTracks.push_back(DeserializeScriptTrack(sj));
    if (j.contains("skeletal")) {
        for (const auto& sj : j["skeletal"]) {
            TimelineClip::SkeletalClipRef sc;
            sc.ClipPath = sj.value("path", "");
            sc.Speed = sj.value("speed", 1.0f);
            sc.Loop = sj.value("loop", true);
            clip.SkeletalClips.push_back(std::move(sc));
        }
    }
    return clip;
}

bool SaveTimelineClip(const TimelineClip& clip, const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << SerializeTimelineClip(clip).dump(4);
    return true;
}

TimelineClip LoadTimelineClip(const std::string& path)
{
    std::ifstream file(path);
    json j; file >> j;
    return DeserializeTimelineClip(j);
}

} // namespace animation
} // namespace cm
