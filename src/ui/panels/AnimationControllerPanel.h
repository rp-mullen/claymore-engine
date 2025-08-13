#pragma once

#include "ui/panels/EditorPanel.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <imgui.h>
#include "ui/panels/InspectorPanel.h"
namespace cm { namespace animation { class AnimatorController; } }


class AnimationControllerPanel : public EditorPanel {
public:
    AnimationControllerPanel();
    ~AnimationControllerPanel();
    void OnImGuiRender();

    // Load/save .animctrl files
    bool Load(const std::string& path);
    bool Save(const std::string& path);

    // Wiring: let panel drive the Inspector when nodes/links are selected
    void SetInspectorPanel(InspectorPanel* inspector) { m_Inspector = inspector; }

private:
    void DrawToolbar();
    void DrawParameterList();
    void DrawNodeEditor();
    void DrawPropertiesPane();

private:
    std::shared_ptr<cm::animation::AnimatorController> m_Controller;
    std::string m_OpenPath;
    int m_NextStateId = 1;
    InspectorPanel* m_Inspector = nullptr;
    // Selection shared across panes
    int m_SelectedStateId = -1;
    int m_SelectedLinkId = -1;
};


