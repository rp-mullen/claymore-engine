// Minimal timeline viewer/editor built on ImSequencer/ImCurveEdit
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "ecs/Scene.h"
#include <imgui.h>
#include "animation/PropertyTrack.h"

class AnimationTimelinePanel {
public:
    AnimationTimelinePanel(Scene* scene, EntityID* selected)
        : m_Scene(scene), m_Selected(selected) {}

    void SetContext(Scene* scene) { m_Scene = scene; }
    void OnImGuiRender();
    bool Load(const std::string& path);
    bool Save(const std::string& path);

    // External integration (Inspector): query and modify the selected property track
    bool HasActivePropertySelection() const { return GetSelectedTrackIndex() >= 0; }
    const std::string& GetSelectedPropertyPath() const { static std::string empty; int ti = GetSelectedTrackIndex(); return ti >= 0 ? m_Clip.Tracks[ti].PropertyPath : empty; }
    float GetCurrentTimeSec() const { return m_TimeSec; }
    float GetFPS() const { return m_FPS; }
    // Add new or update existing key at current time for the selected property track
    bool AddOrUpdateKeyAtCursor(float value);
    bool DeleteKeyNearCursor(float toleranceSec = 1e-3f);

    // Selected key helpers for external inspector
    bool HasSelectedKey() const { return GetSelectedTrackIndex() >= 0 && m_SelectedKeyIndex >= 0; }
    bool GetSelectedKey(cm::animation::KeyframeFloat& outKey) const;
    bool SetSelectedKey(float newTimeSec, float newValue);
    bool RemoveSelectedKey();
    const std::string& GetSelectedTrackName() const { return GetSelectedPropertyPath(); }
    // Clear any current selection so external panels (e.g., Inspector) can revert to entity UI
    void ClearSelection() { m_SelectedEntry = -1; m_SelectedKeyIndex = -1; }

private:
    Scene* m_Scene = nullptr;
    EntityID* m_Selected = nullptr;

    cm::animation::TimelineClip m_Clip;
    std::string m_OpenPath;

    // Sequencer state
    bool m_Expanded = true;
    int m_CurrentFrame = 0;
    int m_FirstFrame = 0;
    float m_FPS = 30.0f;

    // Playback
    bool m_Playing = false;
    float m_TimeSec = 0.0f; // scrubber time
    float m_Zoom = 1.0f;    // reserved for future zoom controls

    // Icons
    bool m_IconsLoaded = false;
    ImTextureID m_IconMove{};
    ImTextureID m_IconRotate{};
    ImTextureID m_IconScale{};
    ImTextureID m_IconEvent{};
    ImTextureID m_IconKey{};

    // Selection
    int m_SelectedEntry = -1;
    int m_SelectedKeyIndex = -1; // within selected entry

    // Drag state for moving keyframes along the timeline
    bool m_IsDraggingKey = false;
    float m_KeyDragStartMouseX = 0.0f;
    float m_KeyDragOriginalTime = 0.0f;

    // Skeletal .anim preview
    bool m_EnableSkeletalPreview = false;
    std::string m_PreviewClipPath;
    std::shared_ptr<cm::animation::AnimationClip> m_PreviewClip;

    // Registered .anim clips (discovered under assets/)
    std::vector<std::string> m_AvailableAnimPaths;
    bool m_AnimListScanned = false;
    // Timeline assets (.animtl)
    std::vector<std::string> m_AvailableTimelinePaths;
    bool m_TimelineListScanned = false;
    std::string m_TimelinePath; // currently opened timeline path

    // Gate for live preview on active selection
    bool m_PreviewOnActive = true;

    void ScanAvailableAnims();
    void ScanAvailableTimelines();

    // --- Helpers to keep UI tidy ---
    void DrawToolbar();
    void DrawAssetHeader();
    void DrawSequencer();
    void DrawPerKeyInspector(int totalFrames);
    void DrawPreviewBlock();

    // Track management
    void EnsureDefaultTracks();
    void DrawTrackManagerUI();
    void AddPropertyTrack(const std::string& propertyPath);
    void AddScriptTrack(const std::string& name = "Script Events");

    // Rendering items for sequencer (segments and event blips)
    struct RenderItem { bool isProp; int trackIndex; int startFrame; int endFrame; unsigned color; };
    std::vector<RenderItem> m_RenderItems;
    void BuildRenderItems();
    int GetSelectedTrackIndex() const;
};

