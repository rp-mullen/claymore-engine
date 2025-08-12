#pragma once

#include <string>

// Lightweight, immediate-mode asset picker widget for editor use
// Lists files under the project asset roots matching a simple glob like "*.anim".
// Supports search filter, recents, and drag-drop acceptance (payload type: "ASSET_FILE").

struct AssetPickerConfig {
    const char* glob = "*.*";
    const char* title = "Assets";
    bool showRecents = true;
};

struct AssetPickerResult {
    bool chosen = false;
    std::string path;
};

// Draws the asset picker UI. Returns chosen path when the user selects an entry or drops a file.
AssetPickerResult DrawAssetPicker(AssetPickerConfig cfg);


