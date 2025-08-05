#pragma once
#include <glm/glm.hpp>
#include "ecs/Scene.h"
#include "editor/Input.h"

class TerrainPainter
{
public:
    // Call every frame from editor when in edit mode
    static void Update(Scene& scene, EntityID selectedEntity);

private:
    static void PaintAt(class TerrainComponent& terrain, uint32_t x, uint32_t y);
};