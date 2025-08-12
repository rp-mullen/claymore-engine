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


