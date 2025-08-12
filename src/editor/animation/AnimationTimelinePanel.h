#pragma once

#include <string>
#include <memory>
#include <imgui.h>
#include "ui/panels/EditorPanel.h"
#include "editor/animation/TimelineDocument.h"
#include "editor/ui/AssetPicker.h"
#include "animation/PreviewContext.h"
#include "animation/PreviewRenderer.h"
#include "animation/BindingCache.h"
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
    void DrawPreviewViewport();

private:
    TimelineDocument m_Doc;
    bool m_Playing = false;
    bool m_PreviewOnActive = false; // OFF by default
    float m_PlaySpeed = 1.0f;

    // Preview resources
    std::unique_ptr<cm::animation::PreviewContext> m_PreviewCtx;
    cm::animation::BindingCache m_Bindings;
    Scene* m_Scene = nullptr;
    EntityID* m_SelectedEntity = nullptr;
};


