#include "animation/AnimationEvaluator.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <algorithm>
#include <unordered_map>
#include "animation/AvatarDefinition.h"
#include "animation/AnimationAsset.h"
#include "animation/BindingCache.h"
#include "animation/Retargeting.h"
#include "ecs/AnimationComponents.h" // for SkeletonComponent::BoneNameToIndex
#include <nlohmann/json.hpp>

namespace cm {
namespace animation {

template<typename KeyContainer>
static size_t FindKeyframeIndex(const KeyContainer& keys, float time, size_t startIdx) {
    // Fast-forward cached index until the next key's time is > time
    while (startIdx + 1 < keys.size() && keys[startIdx + 1].Time < time) {
        ++startIdx;
    }
    return startIdx;
}

glm::vec3 SampleVec3(const std::vector<KeyframeVec3>& keys, float time, size_t& cacheIdx) {
    if (keys.empty()) return glm::vec3(0.0f);
    if (keys.size() == 1) return keys.front().Value;

    cacheIdx = FindKeyframeIndex(keys, time, cacheIdx);

    const auto& k0 = keys[cacheIdx];
    if (cacheIdx + 1 == keys.size()) return k0.Value;

    const auto& k1 = keys[cacheIdx + 1];
    float t = (time - k0.Time) / (k1.Time - k0.Time);
    return glm::mix(k0.Value, k1.Value, t);
}

glm::quat SampleQuat(const std::vector<KeyframeQuat>& keys, float time, size_t& cacheIdx) {
    if (keys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (keys.size() == 1) return keys.front().Value;

    cacheIdx = FindKeyframeIndex(keys, time, cacheIdx);

    const auto& k0 = keys[cacheIdx];
    if (cacheIdx + 1 == keys.size()) return k0.Value;

    const auto& k1 = keys[cacheIdx + 1];
    float t = (time - k0.Time) / (k1.Time - k0.Time);
    return glm::slerp(k0.Value, k1.Value, t);
}

void EvaluateAnimation(const AnimationClip& clip,
                       float time,
                       const ::SkeletonComponent& skeleton,
                       std::vector<glm::mat4>& outLocalTransforms,
                       const AvatarDefinition* avatar) {
    // Ensure output container is sized correctly.
    outLocalTransforms.resize(skeleton.BoneEntities.size(), glm::mat4(1.0f));

    // Cache for each bone track – storing last index to speed up sampling.
    struct CacheEntry { size_t pos = 0; size_t rot = 0; size_t scale = 0; };
    std::unordered_map<std::string, CacheEntry> cache;

    // For every animated bone track in the clip
    for (const auto& [boneName, track] : clip.BoneTracks) {
        const std::string* resolvedName = &boneName;
        std::string tmp;
        // Note: name-to-name indirection is not performed here with AvatarDefinition;
        // proper retargeting is handled by HumanoidRetargeter. Keep generic evaluation.

        // Find bone index in skeleton via name mapping
        int idx = skeleton.GetBoneIndex(*resolvedName);
        if (idx < 0 || static_cast<size_t>(idx) >= skeleton.BoneEntities.size()) {
            continue; // Not found in this skeleton
        }
        size_t boneIdx = static_cast<size_t>(idx);

        CacheEntry& ce = cache[*resolvedName];

        glm::vec3 pos = track.PositionKeys.empty() ? glm::vec3(0.0f) : SampleVec3(track.PositionKeys, time, ce.pos);
        glm::quat rot = track.RotationKeys.empty() ? glm::quat(1,0,0,0) : SampleQuat(track.RotationKeys, time, ce.rot);
        glm::vec3 scl = track.ScaleKeys.empty() ? glm::vec3(1.0f) : SampleVec3(track.ScaleKeys, time, ce.scale);

        glm::mat4 mat = glm::translate(pos) * glm::mat4_cast(rot) * glm::scale(scl);
        outLocalTransforms[boneIdx] = mat;
    }
}

} // namespace animation
} // namespace cm

// ================= Unified Evaluator (new) =================
namespace cm {
namespace animation {

void SampleAsset(const EvalInputs& in, const EvalContext& ctx, EvalTargets& out,
                 std::vector<ScriptEvent>* firedEvents, nlohmann::json* propertyWrites)
{
    if (!in.asset) return;
    const float clipLen = in.asset->Duration();
    float t = in.time;
    if (in.loop && clipLen > 0.0f) t = std::fmod(std::fmod(t, clipLen) + clipLen, clipLen);

    if (out.pose) {
        if (out.pose->touched.size() < out.pose->local.size()) out.pose->touched.resize(out.pose->local.size(), false);
        std::fill(out.pose->touched.begin(), out.pose->touched.end(), false);
    }

    for (const auto& uptr : in.asset->tracks) {
        if (!uptr || uptr->muted) continue;
        const ITrack* base = uptr.get();
        switch (base->type) {
            case TrackType::Avatar: {
                if (!out.pose || !ctx.avatar || !ctx.skeleton) break;
                const auto* at = static_cast<const AssetAvatarTrack*>(base);
                // ctx.avatar is AvatarDefinition now; RetargetAvatarToSkeleton expects HumanoidAvatar
                // Use skeleton's humanoid avatar if available; otherwise skip
                // Note: AvatarDefinition carries mapping; if HumanoidAvatar is required, adapt accordingly
                if (ctx.skeleton->Avatar) {
                    // Convert AvatarDefinition mapping to a transient HumanoidAvatar view
                    HumanoidAvatar hv{};
                    for (const auto& entry : ctx.skeleton->Avatar->Map) {
                        if (ctx.skeleton->Avatar->IsBonePresent(entry.Bone) && !entry.BoneName.empty()) {
                            hv.BoneMapping[(HumanBone)static_cast<int>(entry.Bone)] = entry.BoneName;
                        }
                    }
                    RetargetAvatarToSkeleton(*at, hv, *ctx.skeleton, *out.pose, t, in.loop, clipLen);
                }
            } break;
            case TrackType::Bone: {
                if (!out.pose) break;
                const auto* bt = static_cast<const AssetBoneTrack*>(base);
                glm::vec3 pos = bt->t.keys.empty() ? glm::vec3(0.0f) : bt->t.Sample(t, in.loop, clipLen);
                glm::quat rot = bt->r.keys.empty() ? glm::quat(1,0,0,0) : bt->r.Sample(t, in.loop, clipLen);
                glm::vec3 scl = bt->s.keys.empty() ? glm::vec3(1.0f) : bt->s.Sample(t, in.loop, clipLen);
                int boneIndex = bt->boneId;
                auto tryResolveBone = [&](const std::string& name) -> int {
                    if (!ctx.skeleton) return -1;
                    int idx = ctx.skeleton->GetBoneIndex(name);
                    if (idx >= 0) return idx;
                    // Try suffix after common namespace separators
                    size_t pos = name.find_last_of(':');
                    if (pos != std::string::npos && pos + 1 < name.size()) {
                        idx = ctx.skeleton->GetBoneIndex(name.substr(pos + 1));
                        if (idx >= 0) return idx;
                    }
                    pos = name.find_last_of('|');
                    if (pos != std::string::npos && pos + 1 < name.size()) {
                        idx = ctx.skeleton->GetBoneIndex(name.substr(pos + 1));
                        if (idx >= 0) return idx;
                    }
                    pos = name.find_last_of('.');
                    if (pos != std::string::npos && pos + 1 < name.size()) {
                        idx = ctx.skeleton->GetBoneIndex(name.substr(pos + 1));
                        if (idx >= 0) return idx;
                    }
                    // Final fallback: suffix match against known bone names
                    for (const auto& kv : ctx.skeleton->BoneNameToIndex) {
                        const std::string& skName = kv.first;
                        if (skName.size() >= name.size()) {
                            if (skName.compare(skName.size() - name.size(), name.size(), name) == 0) return kv.second;
                        } else {
                            if (name.compare(name.size() - skName.size(), skName.size(), skName) == 0) return kv.second;
                        }
                    }
                    return -1;
                };
                if (boneIndex < 0 && ctx.skeleton && !base->name.empty()) {
                    boneIndex = tryResolveBone(base->name);
                }
                if (boneIndex >= 0) {
                    const size_t bi = static_cast<size_t>(boneIndex);
                    if (bi >= out.pose->local.size()) out.pose->local.resize(bi + 1, glm::mat4(1.0f));
                    if (bi >= out.pose->touched.size()) out.pose->touched.resize(bi + 1, false);
                    out.pose->local[bi] = glm::translate(pos) * glm::mat4_cast(rot) * glm::scale(scl);
                    out.pose->touched[bi] = true;
                }
            } break;
            case TrackType::Property: {
                if (!propertyWrites || !ctx.bindings) break;
                const auto* pt = static_cast<const AssetPropertyTrack*>(base);
                std::uint64_t id = pt->binding.resolvedId ? pt->binding.resolvedId : ctx.bindings->ResolveProperty(pt->binding.path);
                const std::string key = std::to_string(id);
                switch (pt->binding.type) {
                    case PropertyType::Float: {
                        float v = std::get<CurveFloat>(pt->curve).Sample(t, in.loop, clipLen);
                        (*propertyWrites)[key] = v;
                    } break;
                    case PropertyType::Vec2:  {
                        glm::vec2 v = std::get<CurveVec2>(pt->curve).Sample(t, in.loop, clipLen);
                        (*propertyWrites)[key] = { v.x, v.y };
                    } break;
                    case PropertyType::Vec3:  {
                        glm::vec3 v = std::get<CurveVec3>(pt->curve).Sample(t, in.loop, clipLen);
                        (*propertyWrites)[key] = { v.x, v.y, v.z };
                    } break;
                    case PropertyType::Quat:  {
                        glm::quat v = std::get<CurveQuat>(pt->curve).Sample(t, in.loop, clipLen);
                        (*propertyWrites)[key] = { v.x, v.y, v.z, v.w };
                    } break;
                    case PropertyType::Color: {
                        glm::vec4 v = std::get<CurveColor>(pt->curve).Sample(t, in.loop, clipLen);
                        (*propertyWrites)[key] = { v.x, v.y, v.z, v.w };
                    } break;
                }
            } break;
            case TrackType::ScriptEvent: {
                if (!firedEvents) break;
                const auto* st = static_cast<const AssetScriptEventTrack*>(base);
                for (const auto& e : st->events) {
                    if (std::abs(e.time - t) <= (1.0f / std::max(1.0f, in.asset->meta.fps))) firedEvents->push_back(e);
                }
            } break;
        }
    }
}

} // namespace animation
} // namespace cm

namespace cm { namespace animation {
void SampleAsset(const EvalInputs& in,
                 const EvalContext& ctx,
                 PoseBuffer& outPose,
                 std::vector<ScriptEvent>* outEvents,
                 std::vector<PropertyWrite>* outProps)
{
    EvalTargets tgt{ &outPose };
    nlohmann::json propWrites;
    SampleAsset(in, ctx, tgt, outEvents, outProps ? &propWrites : nullptr);
    if (outProps) {
        outProps->clear();
        for (auto it = propWrites.begin(); it != propWrites.end(); ++it) {
            PropertyWrite w; w.id = std::strtoull(it.key().c_str(), nullptr, 10); w.value = it.value();
            outProps->push_back(std::move(w));
        }
    }
}
} }

