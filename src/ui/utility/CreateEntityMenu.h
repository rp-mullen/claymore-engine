#pragma once
#include <imgui.h>
#include "ecs/Scene.h"

// Draws the common set of Create... menu items used by both the menubar and the hierarchy panel.
// Assumes the caller has already begun an ImGui menu scope (e.g., ImGui::BeginMenu("Create")).
// Returns true if an entity was created and selection was updated via selectedEntityOut.
bool DrawCreateEntityMenuItems(Scene* context, EntityID* selectedEntityOut);


