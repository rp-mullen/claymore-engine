#pragma once

#include "animation/AnimationTypes.h"
#include <nlohmann/json.hpp>
#include "animation/AnimationAsset.h"
#include <string>

namespace cm {
namespace animation {

using json = nlohmann::json;

json SerializeKeyframe(const KeyframeVec3& kf);
json SerializeKeyframe(const KeyframeQuat& kf);
json SerializeKeyframe(const KeyframeFloat& kf);

KeyframeVec3  DeserializeKeyframeVec3(const json& j);
KeyframeQuat  DeserializeKeyframeQuat(const json& j);
KeyframeFloat DeserializeKeyframeFloat(const json& j);

json SerializeAnimationClip(const AnimationClip& clip);
AnimationClip DeserializeAnimationClip(const json& j);

bool SaveAnimationClip(const AnimationClip& clip, const std::string& path);
AnimationClip LoadAnimationClip(const std::string& path);

// Timeline/property tracks (.animtl)
struct TimelineClip;
json SerializeTimelineClip(const TimelineClip& clip);
TimelineClip DeserializeTimelineClip(const json& j);
bool SaveTimelineClip(const TimelineClip& clip, const std::string& path);
TimelineClip LoadTimelineClip(const std::string& path);

// Unified AnimationAsset (versioned)
json SerializeAnimationAsset(const AnimationAsset& asset);
AnimationAsset DeserializeAnimationAsset(const json& j);
bool SaveAnimationAsset(const AnimationAsset& asset, const std::string& path);
AnimationAsset LoadAnimationAsset(const std::string& path);

// Migration: wrap a legacy skeletal AnimationClip as a unified AnimationAsset with BoneTracks
AnimationAsset WrapLegacyClipAsAsset(const AnimationClip& clip);

} // namespace animation
} // namespace cm
