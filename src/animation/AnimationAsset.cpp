#include "animation/AnimationAsset.h"

namespace cm {
namespace animation {

const ITrack* AnimationAsset::FindTrack(TrackID id) const {
    for (const auto& t : tracks) if (t && t->id == id) return t.get();
    return nullptr;
}

ITrack* AnimationAsset::FindTrack(TrackID id) {
    for (auto& t : tracks) if (t && t->id == id) return t.get();
    return nullptr;
}

static float maxTimeInCurves(const AssetPropertyTrack& pt) {
    float m = 0.0f;
    auto scan = [&](auto const& curve){
        for (const auto& k : curve.keys) m = std::max(m, k.t);
    };
    switch (pt.binding.type) {
        case PropertyType::Float: scan(std::get<CurveFloat>(pt.curve)); break;
        case PropertyType::Vec2:  scan(std::get<CurveVec2>(pt.curve)); break;
        case PropertyType::Vec3:  scan(std::get<CurveVec3>(pt.curve)); break;
        case PropertyType::Quat:  scan(std::get<CurveQuat>(pt.curve)); break;
        case PropertyType::Color: scan(std::get<CurveColor>(pt.curve)); break;
    }
    return m;
}

float AnimationAsset::Duration() const {
    if (meta.length > 0.0f) return meta.length;
    float maxT = 0.0f;
    for (const auto& t : tracks) {
        if (!t || t->muted) continue;
        switch (t->type) {
            case TrackType::Bone: {
                const auto* bt = static_cast<const AssetBoneTrack*>(t.get());
                for (const auto& k : bt->t.keys) maxT = std::max(maxT, k.t);
                for (const auto& k : bt->r.keys) maxT = std::max(maxT, k.t);
                for (const auto& k : bt->s.keys) maxT = std::max(maxT, k.t);
            } break;
            case TrackType::Avatar: {
                const auto* at = static_cast<const AssetAvatarTrack*>(t.get());
                for (const auto& k : at->t.keys) maxT = std::max(maxT, k.t);
                for (const auto& k : at->r.keys) maxT = std::max(maxT, k.t);
                for (const auto& k : at->s.keys) maxT = std::max(maxT, k.t);
            } break;
            case TrackType::Property: {
                maxT = std::max(maxT, maxTimeInCurves(*static_cast<const AssetPropertyTrack*>(t.get())));
            } break;
            case TrackType::ScriptEvent: {
                const auto* st = static_cast<const AssetScriptEventTrack*>(t.get());
                for (const auto& e : st->events) maxT = std::max(maxT, e.time);
            } break;
        }
    }
    return maxT;
}

} // namespace animation
} // namespace cm


