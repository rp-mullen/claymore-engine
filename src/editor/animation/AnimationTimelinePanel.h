#pragma once

#include <string>
#include <memory>
#include <imgui.h>
#include "ui/panels/EditorPanel.h"
#include "editor/animation/TimelineDocument.h"
#include "editor/ui/AssetPicker.h"
#include "ui/FileDialogs.h"
#include "ecs/Entity.h" // for Scene and EntityID

// Clean, unified Animation Timeline panel (editor layer)
class AnimTimelinePanel : public EditorPanel {
public:
    AnimTimelinePanel() = default;
    ~AnimTimelinePanel() = default;

    void OnImGuiRender();
    void SetContext(Scene* scene, EntityID* selectedEntity) { m_Scene = scene; m_SelectedEntity = selectedEntity; }

    // External control (e.g., Controller panel)
    bool OpenAsset(const std::string& path);
    const std::string& CurrentPath() const { return m_Doc.path; }

private:
    // Toolbar actions
    void DrawToolbar();
    void DrawTrackTreeAndLanes();
    void DrawInspector();

    // Helpers
    float TimeToX(float t, float duration, float laneX, float laneW) const {
        if (duration <= 0.0f) return laneX;
        return laneX + (t / duration) * laneW;
    }
    float XToTime(float x, float laneX, float laneW, float duration) const {
        if (laneW <= 0.0f) return 0.0f;
        float u = (x - laneX) / laneW; if (u < 0.0f) u = 0.0f; if (u > 1.0f) u = 1.0f; return u * duration;
    }
    float ApplySnap(float t) const {
        if (m_Doc.snapToFrame && m_Doc.fps > 0.0f) {
            float frame = std::round(t * m_Doc.fps);
            return frame / m_Doc.fps;
        }
        if (m_Doc.snapTo01) {
            float step = 0.1f; return std::round(t / step) * step;
        }
        return t;
    }
    void SelectSingleKey(cm::animation::KeyID id) { m_Doc.selectedKeys.clear(); if (id) m_Doc.selectedKeys.push_back(id); }
    void DeselectAll() { m_Doc.ClearSelection(); }

private:
    TimelineDocument m_Doc;
    bool m_Playing = false;
    float m_PlaySpeed = 1.0f;

    Scene* m_Scene = nullptr;
    EntityID* m_SelectedEntity = nullptr;

    // UI state
    cm::animation::KeyID m_HoverKey = 0;
    cm::animation::KeyID m_DragKey = 0;
    float m_DragStartMouseX = 0.0f;
    float m_DragStartTime = 0.0f;
    int m_ContextLaneTrackIndex = -1;
};


