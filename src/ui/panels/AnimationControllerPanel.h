#pragma once

#include "ui/panels/EditorPanel.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <imgui.h>

namespace cm { namespace animation { class AnimatorController; } }

class AnimationControllerPanel : public EditorPanel {
public:
    AnimationControllerPanel();
    ~AnimationControllerPanel();
    void OnImGuiRender();

    // Load/save .animctrl files
    bool Load(const std::string& path);
    bool Save(const std::string& path);

private:
    void DrawToolbar();
    void DrawParameterList();
    void DrawNodeEditor();

private:
    std::shared_ptr<cm::animation::AnimatorController> m_Controller;
    std::string m_OpenPath;
    int m_NextStateId = 1;
};


