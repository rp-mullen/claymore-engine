#include "Terrain.h"

void Terrain::BuildTerrainMesh(TerrainComponent& terrain)
   {
   const uint32_t size = terrain.Size;

   terrain.Vertices.resize(size * size);

   for (uint32_t y = 0; y < size; ++y)
      {
      for (uint32_t x = 0; x < size; ++x)
         {
         uint32_t idx = y * size + x;
         TerrainVertex& vert = terrain.Vertices[idx];
         vert.x = static_cast<float>(x);
         vert.y = static_cast<float>(terrain.HeightMap[idx]);
         vert.z = static_cast<float>(y);

         // Upward normal (will be wrong on cliffs but fine for now)
         vert.nx = 0.0f; vert.ny = 1.0f; vert.nz = 0.0f;

         vert.u = (x + 0.5f) / static_cast<float>(size);
         vert.v = (y + 0.5f) / static_cast<float>(size);
         }
      }

   terrain.Indices.resize((size - 1) * (size - 1) * 6);
   uint32_t index = 0;
   for (uint16_t y = 0; y < size - 1; ++y)
      {
      uint16_t y_offset = y * size;
      for (uint16_t x = 0; x < size - 1; ++x)
         {
         // Use CCW winding when looking from +Y (up) so top faces render
         terrain.Indices[index + 0] = y_offset + x;
         terrain.Indices[index + 1] = y_offset + x + size;
         terrain.Indices[index + 2] = y_offset + x + 1;
         terrain.Indices[index + 3] = y_offset + x + 1;
         terrain.Indices[index + 4] = y_offset + x + size;
         terrain.Indices[index + 5] = y_offset + x + size + 1;
         index += 6;
         }
      }
   }

void Terrain::UpdateTerrainBuffers(TerrainComponent& terrain)
   {
   // Rebuild CPU mesh data
   BuildTerrainMesh(terrain);

   const bgfx::Memory* vMem = bgfx::copy(terrain.Vertices.data(), (uint32_t)(terrain.Vertices.size() * sizeof(TerrainVertex)));
   const bgfx::Memory* iMem = bgfx::copy(terrain.Indices.data(), (uint32_t)(terrain.Indices.size() * sizeof(uint16_t)));

   switch (terrain.Mode)
      {
         default:
         case 0: // Static VB
            if (bgfx::isValid(terrain.vbh)) bgfx::destroy(terrain.vbh);
            if (bgfx::isValid(terrain.ibh)) bgfx::destroy(terrain.ibh);
            terrain.vbh = bgfx::createVertexBuffer(vMem, TerrainVertex::layout);
            terrain.ibh = bgfx::createIndexBuffer(iMem);
            break;
         case 1: // Dynamic VB
            if (!bgfx::isValid(terrain.dvbh))
               terrain.dvbh = bgfx::createDynamicVertexBuffer((uint32_t)terrain.Vertices.size(), TerrainVertex::layout);
            if (!bgfx::isValid(terrain.dibh))
               terrain.dibh = bgfx::createDynamicIndexBuffer((uint32_t)terrain.Indices.size());
            bgfx::update(terrain.dvbh, 0, vMem);
            bgfx::update(terrain.dibh, 0, iMem);
            break;
         case 2: // Height texture mode (not fully implemented yet)
            // Build initial geometry once
            if (!bgfx::isValid(terrain.vbh) || !bgfx::isValid(terrain.ibh))
               {
               terrain.vbh = bgfx::createVertexBuffer(vMem, TerrainVertex::layout);
               terrain.ibh = bgfx::createIndexBuffer(iMem);
               }
            // Update height texture
            if (!bgfx::isValid(terrain.HeightTexture))
               {
               terrain.HeightTexture = bgfx::createTexture2D(terrain.Size, terrain.Size, false, 1, bgfx::TextureFormat::R8);
               }
            const bgfx::Memory* tMem = bgfx::copy(terrain.HeightMap.data(), (uint32_t)terrain.HeightMap.size());
            bgfx::updateTexture2D(terrain.HeightTexture, 0, 0, 0, 0, terrain.Size, terrain.Size, tMem);
            break;
      }
   }
