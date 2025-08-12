// AnimationInspector.cpp
#include "editor/panels/AnimationInspector.h"
#include "ui/UILayer.h"
#include "ui/panels/ProjectPanel.h"
#include "editor/preview/PreviewScene.h"
#include "editor/preview/PreviewAvatarCache.h"
// Keep include here to ensure complete type for unique_ptr destruction in this TU
#include "animation/AnimationPreviewPlayer.h"
#include "animation/AnimationSerializer.h"
#include "pipeline/AssetLibrary.h"
#include <utils/Time.h>
#include <filesystem>

using cm::animation::AnimationClip;

AnimationInspectorPanel::AnimationInspectorPanel(UILayer* uiLayer)
    : m_UILayer(uiLayer)
{
    m_Preview = std::make_unique<PreviewScene>();
    m_AvatarCache = std::make_unique<PreviewAvatarCache>();
    m_Player = std::make_unique<AnimationPreviewPlayer>();
}

AnimationInspectorPanel::~AnimationInspectorPanel() = default;

bool AnimationInspectorPanel::IsVisible() const {
    // Inspector window visibility is handled outside; this panel renders inside it on demand
    return true;
}

void AnimationInspectorPanel::LoadClip(const std::string& path)
{
    m_CurrentClipPath = path;
    // Prefer unified asset; fall back to legacy clip for compatibility
    auto asset = cm::animation::LoadAnimationAsset(path);
    bool loadedAsset = !asset.tracks.empty();
    auto clip = loadedAsset ? cm::animation::AnimationClip{} : cm::animation::LoadAnimationClip(path);
    auto [unusedModel, unusedSkel, humanoid] = m_AvatarCache->ResolveForClip(clip);
    m_Preview->Shutdown();
    m_Preview->Init(480, 320);
    // For humanoid clips, always preview on our default mannequin
    if (clip.IsHumanoid) {
        m_Preview->SetModelPath("assets/prefabs/default_humanoid.fbx");
        m_Preview->ResetCamera();
    }
    if (loadedAsset) {
        m_Player->SetAsset(&asset);
    } else {
        auto clipPtr = std::make_shared<AnimationClip>(clip);
        m_Player->SetClip(clipPtr);
    }
    m_Player->SetSkeleton(m_Preview->GetSkeleton());
    m_Player->SetScene(m_Preview->GetScene());
    // If no skeleton/model is available yet, ensure a default model is loaded
    if (!m_Preview->GetSkeleton()) {
        m_Preview->SetModelPath("assets/prefabs/default_humanoid.fbx");
        m_Player->SetSkeleton(m_Preview->GetSkeleton());
    }
    m_Player->SetLoop(m_Loop);
    m_Player->SetSpeed(m_Speed);
    if (humanoid) m_Player->SetRetargetMap(humanoid);
}

void AnimationInspectorPanel::OnImGuiRender()
{
    // Determine current selection from Project panel
    if (m_UILayer) {
        const std::string selPath = m_UILayer->GetProjectPanel().GetSelectedItemPath();
        std::string ext = std::filesystem::path(selPath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".anim" && selPath != m_CurrentClipPath) {
            LoadClip(selPath);
        }
    }

    // Render only if selected item is an animation
    // Caller should have gated this, but keep a light guard in case
    ImGui::TextUnformatted("Animation");
    ImGui::Separator();

    // Top row controls
    if (ImGui::Checkbox("Play", &m_Playing)) {}
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &m_Loop);
    ImGui::SameLine();
    ImGui::SliderFloat("Speed", &m_Speed, 0.1f, 2.0f, "%.2fx");
    ImGui::Checkbox("Show Bones", &m_ShowBones);
    ImGui::SameLine();
    ImGui::Checkbox("Wireframe", &m_Wireframe);
    ImGui::SameLine();
    ImGui::Checkbox("Auto-rebuild on asset change", &m_AutoRebuildOnChange);

    // Timeline
    float duration = m_Player->GetDuration();
    float t = m_Player->GetTime();
    if (ImGui::SliderFloat(m_ShowFrames ? "Frame" : "Time", &t, 0.0f, std::max(0.001f, duration))) m_Player->SetTime(t);

    // Viewport (bottom region inside the inspector)
    ImGui::Separator();
    ImGui::TextDisabled("Preview");
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float desiredHeight = std::max(140.0f, avail.y * 0.55f);
    ImGui::BeginChild("AnimPreviewViewport", ImVec2(-1, desiredHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImVec2 vpSize = ImGui::GetContentRegionAvail();
    int w = std::max(1, (int)vpSize.x);
    int h = std::max(1, (int)vpSize.y);
    if (w != m_LastKnownWidth || h != m_LastKnownHeight) {
        m_LastKnownWidth = w; m_LastKnownHeight = h;
        m_Preview->Resize(w, h);
    }
    bgfx::TextureHandle tex = m_Preview->GetColorTexture();
    ImVec2 uv0(0, 0), uv1(1, 1);
    if (bgfx::isValid(tex)) {
        // Draw the preview texture fully expanded in the child area
        ImGui::Image((ImTextureID)(uintptr_t)tex.idx, vpSize, uv0, uv1);
    } else {
        ImGui::TextDisabled("(no preview)\nClip or skeleton not loaded yet");
    }

    if (ImGui::IsItemHovered()) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) m_Preview->Orbit(delta.x, delta.y);
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) m_Preview->Pan(delta.x, delta.y);
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) m_Preview->Dolly(wheel);
    }
    ImGui::EndChild();

    bool shouldTick = !ImGui::IsWindowCollapsed();
    if (shouldTick) {
        float dt = (float)Time::GetDeltaTime();
        if (m_Playing) m_Player->Update(dt * m_Speed);
        m_Preview->Render(dt);
    }
}


