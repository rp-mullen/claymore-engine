#include "editor/animation/TimelineDocument.h"
#include "animation/AnimationSerializer.h"

using cm::animation::AnimationAsset;

void TimelineDocument::New()
{
    asset = AnimationAsset{};
    asset.name = "NewAnimation";
    asset.meta.version = 1;
    asset.meta.fps = 30.0f;
    asset.meta.length = 5.0f;
    path.clear();
    time = 0.0f;
    fps = 30.0f;
    loop = true;
    selectedTracks.clear();
    selectedKeys.clear();
    snapToFrame = true;
    snapTo01 = false;
    loopStart = 0.0f;
    loopEnd = 0.0f;
    dirty = true;
}

bool TimelineDocument::Load(const std::string& filePath)
{
    asset = cm::animation::LoadAnimationAsset(filePath);
    if (asset.tracks.empty()) {
        // Back-compat: if a legacy clip is provided, wrap to asset
        auto legacy = cm::animation::LoadAnimationClip(filePath);
        asset = cm::animation::WrapLegacyClipAsAsset(legacy);
    }
    // Ensure all keys have valid IDs for interaction
    ReindexMissingKeyIDs();
    path = filePath;
    time = 0.0f;
    fps = asset.meta.fps > 0.0f ? asset.meta.fps : 30.0f;
    loop = true;
    dirty = false;
    return true;
}

bool TimelineDocument::Save(const std::string& filePath)
{
    bool ok = cm::animation::SaveAnimationAsset(asset, filePath);
    if (ok) { path = filePath; dirty = false; }
    return ok;
}

void TimelineDocument::MarkDirty() { dirty = true; }

float TimelineDocument::Duration() const { return asset.Duration(); }

cm::animation::KeyID TimelineDocument::GenerateKeyID() {
    // Simple monotonically increasing ID; avoids collisions across tracks
    return m_NextKeyId++;
}

void TimelineDocument::ReindexMissingKeyIDs() {
    cm::animation::KeyID maxId = m_NextKeyId;
    for (auto& up : asset.tracks) {
        if (!up) continue;
        using namespace cm::animation;
        switch (up->type) {
            case cm::animation::TrackType::Bone: {
                auto* t = static_cast<cm::animation::AssetBoneTrack*>(up.get());
                for (auto& k : t->t.keys) if (k.id == 0) k.id = GenerateKeyID(); else maxId = std::max(maxId, k.id);
                for (auto& k : t->r.keys) if (k.id == 0) k.id = GenerateKeyID(); else maxId = std::max(maxId, k.id);
                for (auto& k : t->s.keys) if (k.id == 0) k.id = GenerateKeyID(); else maxId = std::max(maxId, k.id);
            } break;
            case cm::animation::TrackType::Avatar: {
                auto* t = static_cast<cm::animation::AssetAvatarTrack*>(up.get());
                for (auto& k : t->t.keys) if (k.id == 0) k.id = GenerateKeyID(); else maxId = std::max(maxId, k.id);
                for (auto& k : t->r.keys) if (k.id == 0) k.id = GenerateKeyID(); else maxId = std::max(maxId, k.id);
                for (auto& k : t->s.keys) if (k.id == 0) k.id = GenerateKeyID(); else maxId = std::max(maxId, k.id);
            } break;
            case cm::animation::TrackType::Property: {
                auto* t = static_cast<cm::animation::AssetPropertyTrack*>(up.get());
                // Visit variant
                auto set = [&](auto& curve){ for (auto& k : curve.keys) if (k.id == 0) k.id = GenerateKeyID(); else maxId = std::max(maxId, k.id); };
                switch (t->binding.type) {
                    case cm::animation::PropertyType::Float: set(std::get<cm::animation::CurveFloat>(t->curve)); break;
                    case cm::animation::PropertyType::Vec2:  set(std::get<cm::animation::CurveVec2>(t->curve)); break;
                    case cm::animation::PropertyType::Vec3:  set(std::get<cm::animation::CurveVec3>(t->curve)); break;
                    case cm::animation::PropertyType::Quat:  set(std::get<cm::animation::CurveQuat>(t->curve)); break;
                    case cm::animation::PropertyType::Color: set(std::get<cm::animation::CurveColor>(t->curve)); break;
                }
            } break;
            case cm::animation::TrackType::ScriptEvent: {
                auto* t = static_cast<cm::animation::AssetScriptEventTrack*>(up.get());
                for (auto& e : t->events) if (e.id == 0) e.id = GenerateKeyID(); else maxId = std::max(maxId, e.id);
            } break;
        }
    }
    m_NextKeyId = std::max(m_NextKeyId, maxId + 1);
}


