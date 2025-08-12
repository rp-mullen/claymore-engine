// InspectorRouter.cpp
#include "ui/UILayer.h"
#include "editor/panels/AnimationInspector.h"
#include "ui/panels/ProjectPanel.h"
#include <imgui.h>

void DrawInspectorRouter(UILayer* ui)
{
    ImGui::Begin("Inspector");
    auto* proj = &ui->GetProjectPanel();
    const std::string ext = proj->GetSelectedItemExtension();
    if (ext == ".anim") {
        if (!ui->GetAnimationInspector()) {
            // Constructed on demand in UILayer; handled elsewhere if needed
        }
        ui->GetAnimationInspector()->OnImGuiRender();
        ImGui::End();
        return;
    }
    // Fallback to default InspectorPanel
    ImGui::End();
}


