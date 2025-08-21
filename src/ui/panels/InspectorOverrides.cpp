#include "prefab/PrefabOverrides.h"
#include <imgui.h>

// Placeholder UI glue for bolding overridden fields later
namespace InspectorOverridesUI {
void DrawOverrideBadge(bool overridden) {
    if (overridden) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,0.3f,0.3f,1));
        ImGui::TextUnformatted("(Overridden)");
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
}
}


