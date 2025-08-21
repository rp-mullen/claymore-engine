#include "animation/SkeletonBinding.h"
#include <imgui.h>

// Minimal badge and placeholder editor for bone remap warnings
namespace BoneRemapUI {
void DrawMissingJointWarning(bool anyMissing) {
    if (!anyMissing) return;
    ImGui::TextColored(ImVec4(1,0.5f,0.2f,1), "Missing Joint: remap incomplete");
}
}


