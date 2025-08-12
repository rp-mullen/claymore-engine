#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <nlohmann/json.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "animation/Curves.h"

namespace cm {
namespace animation {

enum class TrackType { Bone, Avatar, Property, ScriptEvent };

using TrackID = std::uint64_t;
using KeyID   = std::uint64_t;

struct AnimationAssetMeta { int version = 1; float length = 0.0f; float fps = 30.0f; };

struct ITrack {
    TrackID id = 0;
    std::string name;
    TrackType type = TrackType::Bone;
    bool muted = false;
    virtual ~ITrack() = default;
};

struct AssetBoneTrack : ITrack {
    int boneId = -1; // resolved skeleton bone index
    CurveVec3 t; CurveQuat r; CurveVec3 s;
    AssetBoneTrack() { type = TrackType::Bone; }
};

struct AssetAvatarTrack : ITrack {
    int humanBoneId = -1; // canonical humanoid enum value
    CurveVec3 t; CurveQuat r; CurveVec3 s;
    AssetAvatarTrack() { type = TrackType::Avatar; }
};

enum class PropertyType { Float, Vec2, Vec3, Quat, Color };

struct PropertyBinding { std::string path; std::uint64_t resolvedId = 0; PropertyType type = PropertyType::Float; };

struct AssetPropertyTrack : ITrack {
    PropertyBinding binding;
    std::variant<CurveFloat, CurveVec2, CurveVec3, CurveQuat, CurveColor> curve;
    AssetPropertyTrack() { type = TrackType::Property; }
};

struct AssetScriptEvent { KeyID id = 0; float time = 0.0f; std::string className; std::string method; nlohmann::json payload; };

struct AssetScriptEventTrack : ITrack { std::vector<AssetScriptEvent> events; AssetScriptEventTrack() { type = TrackType::ScriptEvent; } };

// Backward-compatible alias used by some parts of the codebase
using ScriptEvent = AssetScriptEvent;

struct AnimationAsset {
    std::string name;
    AnimationAssetMeta meta;
    std::vector<std::unique_ptr<ITrack>> tracks;

    const ITrack* FindTrack(TrackID id) const;
    ITrack* FindTrack(TrackID id);
    float Duration() const; // derived from max key time across all tracks if meta.length == 0
};

} // namespace animation
} // namespace cm


