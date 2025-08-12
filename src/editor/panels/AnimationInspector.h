// AnimationInspector.h
#pragma once

#include <string>
#include <memory>
#include <bgfx/bgfx.h>
#include <imgui.h>

namespace cm { namespace animation { struct AnimationClip; struct AnimationAsset; } }
class UILayer;

class PreviewScene;
class PreviewAvatarCache;
#include "animation/AnimationPreviewPlayer.h"

class AnimationInspectorPanel {
public:
    explicit AnimationInspectorPanel(UILayer* uiLayer);
    ~AnimationInspectorPanel();

    void OnImGuiRender();

private:
    void LoadClip(const std::string& path);
    bool IsVisible() const;

private:
    UILayer* m_UILayer = nullptr; // non-owning
    std::unique_ptr<PreviewScene> m_Preview;
    std::unique_ptr<PreviewAvatarCache> m_AvatarCache;
    std::unique_ptr<AnimationPreviewPlayer> m_Player;

    std::string m_CurrentClipPath;
    bool m_Playing = true;
    bool m_Loop = true;
    bool m_ShowBones = false;
    bool m_Wireframe = false;
    bool m_AutoRebuildOnChange = true;
    float m_Speed = 1.0f;
    bool m_ShowFrames = false;
    int m_LastKnownWidth = 0;
    int m_LastKnownHeight = 0;
};


