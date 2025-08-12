#include "animation/AnimationSerializer.h"
#include <fstream>
#include "animation/PropertyTrack.h"
#include "animation/AnimationAsset.h"
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

// Backward-compat loader: if file looks like legacy skeletal .anim (our JSON shape), allow wrapping to unified asset if requested elsewhere.

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

// ---------------- Unified AnimationAsset (v1) ----------------
json SerializeAnimationAsset(const AnimationAsset& asset)
{
    json j;
    j["meta"] = { {"version", asset.meta.version}, {"length", asset.meta.length}, {"fps", asset.meta.fps} };
    j["name"] = asset.name;
    j["tracks"] = json::array();
    for (const auto& t : asset.tracks) {
        if (!t) continue;
        json jt; jt["id"] = t->id; jt["name"] = t->name; jt["muted"] = t->muted;
        switch (t->type) {
            case TrackType::Bone: {
                jt["type"] = "Bone";
                const auto* bt = static_cast<const AssetBoneTrack*>(t.get());
                jt["boneId"] = bt->boneId;
                auto dumpCurveVec3 = [](CurveVec3 const& c){ json a = json::array(); for (auto const& k : c.keys) a.push_back({{"id", k.id}, {"t", k.t}, {"v", {k.v.x, k.v.y, k.v.z}}}); return a; };
                auto dumpCurveQuat = [](CurveQuat const& c){ json a = json::array(); for (auto const& k : c.keys) a.push_back({{"id", k.id}, {"t", k.t}, {"v", {k.v.x, k.v.y, k.v.z, k.v.w}}}); return a; };
                jt["t"] = dumpCurveVec3(bt->t);
                jt["r"] = dumpCurveQuat(bt->r);
                jt["s"] = dumpCurveVec3(bt->s);
            } break;
            case TrackType::Avatar: {
                jt["type"] = "Avatar";
                const auto* at = static_cast<const AssetAvatarTrack*>(t.get());
                jt["humanBoneId"] = at->humanBoneId;
                auto dumpCurveVec3 = [](CurveVec3 const& c){ json a = json::array(); for (auto const& k : c.keys) a.push_back({{"id", k.id}, {"t", k.t}, {"v", {k.v.x, k.v.y, k.v.z}}}); return a; };
                auto dumpCurveQuat = [](CurveQuat const& c){ json a = json::array(); for (auto const& k : c.keys) a.push_back({{"id", k.id}, {"t", k.t}, {"v", {k.v.x, k.v.y, k.v.z, k.v.w}}}); return a; };
                jt["t"] = dumpCurveVec3(at->t);
                jt["r"] = dumpCurveQuat(at->r);
                jt["s"] = dumpCurveVec3(at->s);
            } break;
            case TrackType::Property: {
                jt["type"] = "Property";
                const auto* pt = static_cast<const AssetPropertyTrack*>(t.get());
                jt["binding"] = { {"path", pt->binding.path}, {"resolvedId", pt->binding.resolvedId}, {"ptype", (int)pt->binding.type} };
                switch (pt->binding.type) {
                    case PropertyType::Float: { json a = json::array(); for (auto const& k : std::get<CurveFloat>(pt->curve).keys) a.push_back({{"id", k.id},{"t",k.t},{"v",k.v}}); jt["curve"] = a; } break;
                    case PropertyType::Vec2:  { json a = json::array(); for (auto const& k : std::get<CurveVec2>(pt->curve).keys)  a.push_back({{"id", k.id},{"t",k.t},{"v", {k.v.x, k.v.y}}}); jt["curve"] = a; } break;
                    case PropertyType::Vec3:  { json a = json::array(); for (auto const& k : std::get<CurveVec3>(pt->curve).keys)  a.push_back({{"id", k.id},{"t",k.t},{"v", {k.v.x, k.v.y, k.v.z}}}); jt["curve"] = a; } break;
                    case PropertyType::Quat:  { json a = json::array(); for (auto const& k : std::get<CurveQuat>(pt->curve).keys)  a.push_back({{"id", k.id},{"t",k.t},{"v", {k.v.x, k.v.y, k.v.z, k.v.w}}}); jt["curve"] = a; } break;
                    case PropertyType::Color: { json a = json::array(); for (auto const& k : std::get<CurveColor>(pt->curve).keys) a.push_back({{"id", k.id},{"t",k.t},{"v", {k.v.x, k.v.y, k.v.z, k.v.w}}}); jt["curve"] = a; } break;
                }
            } break;
            case TrackType::ScriptEvent: {
                jt["type"] = "ScriptEvent";
                const auto* st = static_cast<const AssetScriptEventTrack*>(t.get());
                json arr = json::array();
                for (const auto& e : st->events) arr.push_back({{"id", e.id}, {"t", e.time}, {"class", e.className}, {"method", e.method}, {"payload", e.payload}});
                jt["events"] = std::move(arr);
            } break;
        }
        j["tracks"].push_back(std::move(jt));
    }
    return j;
}

static void readCurve(json const& arr, CurveFloat& c){ for (auto const& k : arr) c.keys.push_back({k.value("id",0ull), k.value("t",0.f), k.value("v",0.f)}); }
static void readCurve(const json& arr, CurveVec2& c){ for (const auto& k : arr) c.keys.push_back({k.value("id",0ull), k.value("t",0.f), glm::vec2(k["v"][0].get<float>(), k["v"][1].get<float>())}); }
static void readCurve(const json& arr, CurveVec3& c){ for (const auto& k : arr) c.keys.push_back({k.value("id",0ull), k.value("t",0.f), glm::vec3(k["v"][0].get<float>(), k["v"][1].get<float>(), k["v"][2].get<float>())}); }
static void readCurve(const json& arr, CurveQuat& c){ for (const auto& k : arr) c.keys.push_back({k.value("id",0ull), k.value("t",0.f), glm::quat(k["v"][3].get<float>(), k["v"][0].get<float>(), k["v"][1].get<float>(), k["v"][2].get<float>())}); }
static void readCurve(const json& arr, CurveColor& c){ for (const auto& k : arr) c.keys.push_back({k.value("id",0ull), k.value("t",0.f), glm::vec4(k["v"][0].get<float>(), k["v"][1].get<float>(), k["v"][2].get<float>(), k["v"][3].get<float>())}); }

AnimationAsset DeserializeAnimationAsset(const json& j)
{
    AnimationAsset a; a.name = j.value("name", "");
    if (j.contains("meta")) { a.meta.version = j["meta"].value("version", 1); a.meta.length = j["meta"].value("length", 0.0f); a.meta.fps = j["meta"].value("fps", 30.0f); }
    if (j.contains("tracks")) {
        for (const auto& jt : j["tracks"]) {
            const std::string tt = jt.value("type", "");
            if (tt == "Bone") {
                auto t = std::make_unique<AssetBoneTrack>();
                t->type = TrackType::Bone; t->id = jt.value("id", 0ull); t->name = jt.value("name", ""); t->muted = jt.value("muted", false); t->boneId = jt.value("boneId", -1);
                if (jt.contains("t")) readCurve(jt["t"], t->t);
                if (jt.contains("r")) readCurve(jt["r"], t->r);
                if (jt.contains("s")) readCurve(jt["s"], t->s);
                a.tracks.push_back(std::move(t));
            } else if (tt == "Avatar") {
                auto t = std::make_unique<AssetAvatarTrack>();
                t->type = TrackType::Avatar; t->id = jt.value("id", 0ull); t->name = jt.value("name", ""); t->muted = jt.value("muted", false); t->humanBoneId = jt.value("humanBoneId", -1);
                if (jt.contains("t")) readCurve(jt["t"], t->t);
                if (jt.contains("r")) readCurve(jt["r"], t->r);
                if (jt.contains("s")) readCurve(jt["s"], t->s);
                a.tracks.push_back(std::move(t));
            } else if (tt == "Property") {
                auto t = std::make_unique<AssetPropertyTrack>();
                t->type = TrackType::Property; t->id = jt.value("id", 0ull); t->name = jt.value("name", ""); t->muted = jt.value("muted", false);
                if (jt.contains("binding")) { t->binding.path = jt["binding"].value("path", ""); t->binding.resolvedId = jt["binding"].value("resolvedId", 0ull); t->binding.type = static_cast<PropertyType>(jt["binding"].value("ptype", 0)); }
                if (jt.contains("curve")) {
                    switch (t->binding.type) {
                        case PropertyType::Float: { CurveFloat c; readCurve(jt["curve"], c); t->curve = std::move(c); } break;
                        case PropertyType::Vec2:  { CurveVec2  c; readCurve(jt["curve"], c); t->curve = std::move(c); } break;
                        case PropertyType::Vec3:  { CurveVec3  c; readCurve(jt["curve"], c); t->curve = std::move(c); } break;
                        case PropertyType::Quat:  { CurveQuat  c; readCurve(jt["curve"], c); t->curve = std::move(c); } break;
                        case PropertyType::Color: { CurveColor c; readCurve(jt["curve"], c); t->curve = std::move(c); } break;
                    }
                }
                a.tracks.push_back(std::move(t));
            } else if (tt == "ScriptEvent") {
                auto t = std::make_unique<AssetScriptEventTrack>();
                t->type = TrackType::ScriptEvent; t->id = jt.value("id", 0ull); t->name = jt.value("name", ""); t->muted = jt.value("muted", false);
                if (jt.contains("events")) {
                    for (const auto& ej : jt["events"]) {
                        AssetScriptEvent e; e.id = ej.value("id", 0ull); e.time = ej.value("t", 0.0f); e.className = ej.value("class", ""); e.method = ej.value("method", ""); e.payload = ej.value("payload", json{});
                        t->events.push_back(std::move(e));
                    }
                }
                a.tracks.push_back(std::move(t));
            }
        }
    }
    return a;
}

bool SaveAnimationAsset(const AnimationAsset& asset, const std::string& path)
{
    std::ofstream f(path); if (!f.is_open()) return false; f << SerializeAnimationAsset(asset).dump(4); return true;
}

AnimationAsset LoadAnimationAsset(const std::string& path)
{
    std::ifstream f(path); if (!f.is_open()) return AnimationAsset{}; json j; f >> j; return DeserializeAnimationAsset(j);
}

AnimationAsset WrapLegacyClipAsAsset(const AnimationClip& clip)
{
    AnimationAsset a; a.name = clip.Name; a.meta.version = 1; a.meta.fps = (clip.TicksPerSecond > 0.0f) ? clip.TicksPerSecond : 30.0f; a.meta.length = clip.Duration;
    for (const auto& kv : clip.BoneTracks) {
        const std::string& boneName = kv.first;
        const BoneTrack& bt = kv.second;
        auto t = std::make_unique<AssetBoneTrack>();
        t->name = boneName;
        for (const auto& k : bt.PositionKeys) t->t.keys.push_back({0ull, k.Time, k.Value});
        for (const auto& k : bt.RotationKeys) t->r.keys.push_back({0ull, k.Time, k.Value});
        for (const auto& k : bt.ScaleKeys)    t->s.keys.push_back({0ull, k.Time, k.Value});
        a.tracks.push_back(std::move(t));
    }
    // Optionally include humanoid tracks by mapping to AvatarTrack later
    return a;
}

} // namespace animation
} // namespace cm
