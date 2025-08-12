#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include "animation/AnimationAsset.h"

// In-memory document model for the editor timeline
class TimelineDocument {
public:
    // Data
    cm::animation::AnimationAsset asset;
    std::string path; // empty means unsaved

    // Editor state
    bool dirty = false;
    float time = 0.0f; // seconds
    float fps = 30.0f;
    bool loop = true;

    // Selection / editing state (MVP)
    std::vector<cm::animation::TrackID> selectedTracks;
    std::vector<cm::animation::KeyID> selectedKeys;
    bool snapToFrame = true;
    bool snapTo01 = false; // 0.1s
    float loopStart = 0.0f;
    float loopEnd = 0.0f;

    // API
    void New();
    bool Load(const std::string& filePath);
    bool Save(const std::string& filePath);
    void MarkDirty();
    float Duration() const;

    // Key utilities used by the editor
    cm::animation::KeyID GenerateKeyID();
    void ReindexMissingKeyIDs();
    void ClearSelection() { selectedTracks.clear(); selectedKeys.clear(); }

private:
    cm::animation::KeyID m_NextKeyId = 1;
};


