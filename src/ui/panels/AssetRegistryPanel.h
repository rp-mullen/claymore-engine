#pragma once
#include "EditorPanel.h"
#include "pipeline/AssetLibrary.h"
#include <imgui.h>

class AssetRegistryPanel : public EditorPanel {
public:
    AssetRegistryPanel() = default;
    ~AssetRegistryPanel() = default;

    void OnImGuiRender();
};


