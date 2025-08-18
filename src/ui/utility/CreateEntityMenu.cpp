#include "CreateEntityMenu.h"
#include <rendering/StandardMeshManager.h>
#include <rendering/MaterialManager.h>

bool DrawCreateEntityMenuItems(Scene* context, EntityID* selectedEntityOut)
{
    if (!context || !selectedEntityOut) return false;
    bool created = false;

    if (ImGui::MenuItem("Empty")) {
        auto e = context->CreateEntity("Empty Entity");
        *selectedEntityOut = e.GetID();
        created = true;
    }

    if (ImGui::MenuItem("Camera")) {
        auto e = context->CreateEntity("Camera");
        if (auto* d = context->GetEntityData(e.GetID())) {
            d->Camera = std::make_unique<CameraComponent>();
        }
        *selectedEntityOut = e.GetID();
        created = true;
    }

    if (ImGui::MenuItem("Cube")) {
        auto e = context->CreateEntity("Cube");
        if (auto* d = context->GetEntityData(e.GetID())) {
            d->Mesh = std::make_unique<MeshComponent>();
            d->Mesh->mesh = StandardMeshManager::Instance().GetCubeMesh();
            d->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();
            d->Mesh->MeshName = "Cube";
        }
        *selectedEntityOut = e.GetID();
        created = true;
    }

    if (ImGui::MenuItem("Sphere")) {
        auto e = context->CreateEntity("Sphere");
        if (auto* d = context->GetEntityData(e.GetID())) {
            d->Mesh = std::make_unique<MeshComponent>();
            d->Mesh->mesh = StandardMeshManager::Instance().GetSphereMesh();
            d->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();
            d->Mesh->MeshName = "Sphere";
        }
        *selectedEntityOut = e.GetID();
        created = true;
    }

    if (ImGui::MenuItem("Plane")) {
        auto e = context->CreateEntity("Plane");
        if (auto* d = context->GetEntityData(e.GetID())) {
            d->Mesh = std::make_unique<MeshComponent>();
            d->Mesh->mesh = StandardMeshManager::Instance().GetPlaneMesh();
            d->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();
            d->Mesh->MeshName = "Plane";
        }
        *selectedEntityOut = e.GetID();
        created = true;
    }

    if (ImGui::BeginMenu("Light")) {
        if (ImGui::MenuItem("Directional")) {
            auto e = context->CreateEntity("Directional Light");
            if (auto* d = context->GetEntityData(e.GetID())) {
                d->Light = std::make_unique<LightComponent>(LightType::Directional, glm::vec3(1.0f), 1.0f);
            }
            *selectedEntityOut = e.GetID();
            created = true;
        }
        if (ImGui::MenuItem("Point")) {
            auto e = context->CreateEntity("Point Light");
            if (auto* d = context->GetEntityData(e.GetID())) {
                d->Light = std::make_unique<LightComponent>(LightType::Point, glm::vec3(1.0f), 1.0f);
            }
            *selectedEntityOut = e.GetID();
            created = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Terrain")) {
        auto e = context->CreateEntity("Terrain");
        if (auto* d = context->GetEntityData(e.GetID())) {
            d->Terrain = std::make_unique<TerrainComponent>();
            d->Transform.Position = glm::vec3(-0.5f * d->Terrain->Size, 0.0f, -0.5f * d->Terrain->Size);
        }
        *selectedEntityOut = e.GetID();
        created = true;
    }

    if (ImGui::MenuItem("Particle Emitter")) {
        auto e = context->CreateEntity("Particle Emitter");
        if (auto* d = context->GetEntityData(e.GetID())) {
            d->Emitter = std::make_unique<ParticleEmitterComponent>();
        }
        *selectedEntityOut = e.GetID();
        created = true;
    }

    if (ImGui::BeginMenu("UI")) {
        if (ImGui::MenuItem("Canvas")) {
            auto e = context->CreateEntity("Canvas");
            if (auto* d = context->GetEntityData(e.GetID())) {
                d->Canvas = std::make_unique<CanvasComponent>();
            }
            *selectedEntityOut = e.GetID();
            created = true;
        }
        if (ImGui::MenuItem("Panel")) {
            auto e = context->CreateEntity("Panel");
            if (auto* d = context->GetEntityData(e.GetID())) {
                d->Panel = std::make_unique<PanelComponent>();
            }
            *selectedEntityOut = e.GetID();
            created = true;
        }
        if (ImGui::MenuItem("Button")) {
            auto e = context->CreateEntity("Button");
            if (auto* d = context->GetEntityData(e.GetID())) {
                d->Panel = std::make_unique<PanelComponent>();
                d->Button = std::make_unique<ButtonComponent>();
            }
            *selectedEntityOut = e.GetID();
            created = true;
        }
        if (ImGui::MenuItem("Text")) {
            auto e = context->CreateEntity("Text");
            if (auto* d = context->GetEntityData(e.GetID())) {
                d->Text = std::make_unique<TextRendererComponent>();
                d->Text->WorldSpace = false;
            }
            *selectedEntityOut = e.GetID();
            created = true;
        }
        ImGui::EndMenu();
    }

    return created;
}


