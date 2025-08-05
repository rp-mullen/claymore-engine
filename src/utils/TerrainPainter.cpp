#include "TerrainPainter.h"
#include "rendering/Renderer.h"
#include <imgui.h>
#include "ecs/Components.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <bx/math.h>

static void PaintHeight(TerrainComponent& terrain, uint32_t x, uint32_t y)
{
    const int32_t size = terrain.Size;
    const TerrainBrush& brush = terrain.Brush;

    for (int32_t by = -brush.Size; by < brush.Size; ++by)
    {
        for (int32_t bx = -brush.Size; bx < brush.Size; ++bx)
        {
            int32_t px = (int32_t)x + bx;
            int32_t py = (int32_t)y + by;
            if (px < 0 || px >= size || py < 0 || py >= size)
                continue;

            uint32_t idx = py * size + px;
            float height = (float)terrain.HeightMap[idx];

            float a2 = (float)(bx * bx);
            float b2 = (float)(by * by);
            float attn = brush.Size - sqrtf(a2 + b2);
            float delta = attn * brush.Power;
            if (delta < 0.0f) delta = 0.0f;
            height += (brush.Raise ? 1.0f : -1.0f) * delta;

            height = glm::clamp(height, 0.0f, 255.0f);
            terrain.HeightMap[idx] = (uint8_t)height;
        }
    }

    terrain.Dirty = true;
}

void TerrainPainter::Update(Scene& scene, EntityID selectedEntity)
{
    if (selectedEntity == 0) return;
    auto* data = scene.GetEntityData(selectedEntity);
    if (!data || !data->Terrain) return;
    TerrainComponent& terrain = *data->Terrain;
    if (!terrain.PaintMode) return;

    // Require left mouse button
    if (!Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) return;
    // Avoid when ImGui wants mouse
    if (ImGui::GetIO().WantCaptureMouse) return;

    // Build ray from mouse screen position
    ImVec2 mousePos = ImGui::GetMousePos();
    Renderer& renderer = Renderer::Get();
    int width = renderer.GetWidth();
    int height = renderer.GetHeight();
    if (width == 0 || height == 0) return;

    float ndcX = (2.0f * mousePos.x) / width - 1.0f;
    float ndcY = 1.0f - (2.0f * mousePos.y) / height;

    glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);

    Camera* cam = renderer.GetCamera();
    glm::mat4 proj = cam->GetProjectionMatrix();
    glm::mat4 view = cam->GetViewMatrix();

    glm::mat4 invProj = glm::inverse(proj);
    glm::mat4 invView = glm::inverse(view);

    glm::vec4 rayEye = invProj * rayClip;
    rayEye.z = -1.0f;
    rayEye.w = 0.0f;

    glm::vec3 rayDir = glm::normalize(glm::vec3(invView * rayEye));
    glm::vec3 rayOrigin = cam->GetPosition();

    // Transform to terrain local space
    glm::mat4 invTerrainMtx = glm::inverse(data->Transform.WorldMatrix);

    glm::vec3 dirLocal = glm::vec3(invTerrainMtx * glm::vec4(rayDir, 0.0f));
    glm::vec3 origLocal = glm::vec3(invTerrainMtx * glm::vec4(rayOrigin, 1.0f));

    const int maxSteps = 1000;
    glm::vec3 pos = origLocal;
    for (int i = 0; i < maxSteps; ++i)
    {
        pos += dirLocal;
        if (pos.x < 0 || pos.x >= terrain.Size || pos.z < 0 || pos.z >= terrain.Size)
            continue;
        uint32_t idx = (uint32_t)pos.z * terrain.Size + (uint32_t)pos.x;
        if (pos.y < terrain.HeightMap[idx])
        {
            PaintHeight(terrain, (uint32_t)pos.x, (uint32_t)pos.z);
            break;
        }
    }
}
