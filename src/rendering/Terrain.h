#pragma once
#include <ecs/Components.h>

class Terrain {

public:
   static void BuildTerrainMesh(TerrainComponent& terrain);
   static void UpdateTerrainBuffers(TerrainComponent& terrain);
   static void DrawTerrain(const TerrainComponent& terrain, const float* transform, uint16_t viewId = 0);

   };