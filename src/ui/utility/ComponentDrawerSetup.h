#pragma once
#include "ComponentDrawerRegistry.h"
#include "ecs/Components.h"
#include "ecs/AnimationComponents.h" // adjust path to where TransformComponent etc. live
#include "rendering/TextureLoader.h"
#include "rendering/Renderer.h"
#include "particles/SpriteLoader.h"
#include "physics/Physics.h"

#include <imgui.h>
#include <glm/glm.hpp>
#include <filesystem>
#include <algorithm>
#include <particles/ParticleSystem.h>

inline void RegisterComponentDrawers() {
    auto& registry = ComponentDrawerRegistry::Instance();

    registry.Register<TransformComponent>("Transform", [](TransformComponent& t) {
        bool dirty = false;
        dirty |= ImGui::DragFloat3("Position", &t.Position.x, 0.1f);
        dirty |= ImGui::DragFloat3("Rotation", &t.Rotation.x, 0.1f);
        dirty |= ImGui::DragFloat3("Scale", &t.Scale.x, 0.1f);
        if (dirty) t.TransformDirty = true;
        });

    registry.Register<MeshComponent>("Mesh", [](MeshComponent& m) {
        ImGui::Text("Mesh Name: %s", m.MeshName.c_str());
        if (!m.material) return;
        ImGui::Text("Material: %s", m.material->GetName().c_str());

        // Attempt to cast to PBRMaterial to expose texture slots
        if (auto pbr = std::dynamic_pointer_cast<PBRMaterial>(m.material)) {
            auto drawTexSlot = [&](const char* label, bgfx::TextureHandle& tex) {
                ImGui::Text("%s", label);
                ImGui::SameLine();

                ImTextureID texId = (ImTextureID)(uintptr_t)(bgfx::isValid(tex) ? tex.idx : 0);
                ImVec2 size(64, 64);
                ImGui::Image(texId, size, ImVec2(0,0), ImVec2(1,1));

                // Drag-drop target for texture assignment
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
                        const char* path = (const char*)payload->Data;
                        std::string ext = std::filesystem::path(path).extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
                            bgfx::TextureHandle newTex = TextureLoader::Load2D(path);
                            if (bgfx::isValid(newTex)) {
                                tex = newTex;
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            };

            drawTexSlot("Albedo", pbr->m_AlbedoTex);
            drawTexSlot("MetallicRoughness", pbr->m_MetallicRoughnessTex);
            drawTexSlot("Normal", pbr->m_NormalTex);
        }
        // Blend shape sliders
        if (m.BlendShapes) {
            if (ImGui::CollapsingHeader("Blend Shapes")) {
                for (auto& shape : m.BlendShapes->Shapes) {
                    if (ImGui::SliderFloat(shape.Name.c_str(), &shape.Weight, 0.0f, 1.0f)) {
                        m.BlendShapes->Dirty = true;
                    }
                }
            }
        }
        });

    registry.Register<LightComponent>("Light", [](LightComponent& l) {
        int type = static_cast<int>(l.Type);
        const char* types[] = { "Directional", "Point" };
        ImGui::Combo("Type", &type, types, IM_ARRAYSIZE(types));
        l.Type = static_cast<LightType>(type);

        ImGui::ColorEdit3("Color", &l.Color.x);
        ImGui::DragFloat("Intensity", &l.Intensity, 0.05f, 0.0f, 100.0f);
        });

    registry.Register<ColliderComponent>("Collider", [](ColliderComponent& c) {
        // Shape type dropdown
        int shapeType = static_cast<int>(c.ShapeType);
        const char* shapeTypes[] = { "Box", "Capsule", "Mesh" };
        if (ImGui::Combo("Shape Type", &shapeType, shapeTypes, IM_ARRAYSIZE(shapeTypes))) {
            c.ShapeType = static_cast<ColliderShape>(shapeType);
        }

        // Common properties
        ImGui::DragFloat3("Offset", &c.Offset.x, 0.1f);
        ImGui::Checkbox("Is Trigger", &c.IsTrigger);

        // Shape-specific properties
        switch (c.ShapeType) {
            case ColliderShape::Box:
                ImGui::DragFloat3("Size", &c.Size.x, 0.1f, 0.01f, 100.0f);
                break;
            case ColliderShape::Capsule:
                ImGui::DragFloat("Radius", &c.Radius, 0.01f, 0.01f, 10.0f);
                ImGui::DragFloat("Height", &c.Height, 0.01f, 0.01f, 20.0f);
                break;
            case ColliderShape::Mesh:
                ImGui::Text("Mesh Path: %s", c.MeshPath.empty() ? "(None)" : c.MeshPath.c_str());
                // TODO: Add mesh path selection
                break;
        }
        });

#if 0
    registry.Register<ParticleSystemComponent>("ParticleSystem", [](ParticleSystemComponent& ps) {
        ImGui::DragFloat("Emission Rate", &ps.EmissionRate, 1.0f, 0.0f, 100000.0f);
        ImGui::DragFloat2("Lifetime Min/Max", &ps.LifetimeMin, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat3("Gravity", &ps.Gravity.x, 0.1f, -100.0f, 100.0f);
        ImGui::DragFloat("Start Size", &ps.StartSize, 0.01f, 0.0f, 100.0f);
        ImGui::DragFloat("End Size", &ps.EndSize, 0.01f, 0.0f, 100.0f);
        ImGui::ColorEdit4("Start Color", &ps.StartColor.x);
        ImGui::ColorEdit4("End Color", &ps.EndColor.x);

        ImGui::Separator();
        ImGui::Text("Particle Texture");
        ImGui::SameLine();
        ImTextureID texId = (ImTextureID)(uintptr_t)(bgfx::isValid(ps.Texture) ? ps.Texture.idx : 0);
        ImVec2 size(64, 64);
        ImGui::Image(texId, size);

        // Drag-drop target for assigning texture
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
                const char* path = (const char*)payload->Data;
                std::string ext = std::filesystem::path(path).extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
                    bgfx::TextureHandle newTex = TextureLoader::Load2D(path);
                    if (bgfx::isValid(newTex)) {
                        ps.Texture = newTex;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    });
#endif

    // New ParticleEmitter drawer
    registry.Register<ParticleEmitterComponent>("ParticleEmitter", [](ParticleEmitterComponent& e) {
        ImGui::DragInt("Particles/Second", (int*)&e.Uniforms.m_particlesPerSecond, 1, 0, 100000);
        ImGui::DragFloat3("Position", e.Uniforms.m_position, 0.1f);
        ImGui::Checkbox("Enabled", &e.Enabled);

        ImGui::Separator();
        ImGui::Text("Sprite");
        ImGui::SameLine();
        ImVec2 preview(48,48);

        ImTextureID imgID = 0;
        float uv[4];
        if (ps::isValid(e.SpriteHandle) && ps::GetSpriteUV(e.SpriteHandle, uv))
        {
            imgID = (ImTextureID)(uintptr_t)(bgfx::isValid(ps::GetTexture()) ? ps::GetTexture().idx : 0);
            ImGui::Image(imgID, preview, ImVec2(uv[0], uv[1]), ImVec2(uv[2], uv[3]));
        }
        else
        {
            ImGui::TextDisabled("(None)");
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
            {
                const char* path = (const char*)payload->Data;
                std::string ext = (std::filesystem::path(path).extension().string());
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga")
                {
                    auto sprite = particles::LoadSprite(path);
                    if (ps::isValid(sprite))
                    {
                        e.SpriteHandle = sprite;
                        e.Uniforms.m_handle = sprite;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    });

    registry.Register<TerrainComponent>("Terrain", [](TerrainComponent& t) {
        bool dirty = false;
        int mode = t.Mode;
        const char* modes[] = { "Vertex Buffer", "Dynamic Vertex Buffer", "Height Texture" };
        if (ImGui::Combo("Mode", &mode, modes, IM_ARRAYSIZE(modes))) {
            t.Mode = mode;
            t.Dirty = true;
        }

        ImGui::Checkbox("Raise Terrain", &t.Brush.Raise);
        ImGui::DragInt("Brush Size", &t.Brush.Size, 1, 1, 50);
        ImGui::DragFloat("Brush Power", &t.Brush.Power, 0.01f, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::Checkbox("Paint Mode", &t.PaintMode);
        });


    registry.Register<CameraComponent>("Camera", [](CameraComponent& c) {
        ImGui::Checkbox("Active", &c.Active);
        ImGui::DragInt("Priority", &c.priority, 1, 0, 100);
        ImGui::Separator();
        
        ImGui::Text("Projection Settings:");
        ImGui::DragFloat("Field of View", &c.FieldOfView, 1.0f, 1.0f, 179.0f);
        ImGui::DragFloat("Near Clip", &c.NearClip, 0.01f, 0.01f, 100.0f);
        ImGui::DragFloat("Far Clip", &c.FarClip, 1.0f, 1.0f, 10000.0f);
        ImGui::Checkbox("Perspective", &c.IsPerspective);
        
        // Update the camera's projection when parameters change
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            float aspectRatio = (float)Renderer::Get().GetWidth() / (float)Renderer::Get().GetHeight();
            c.UpdateProjection(aspectRatio);
        }
    });

    registry.Register<RigidBodyComponent>("RigidBody", [](RigidBodyComponent& rb) {
        ImGui::DragFloat("Mass", &rb.Mass, 0.1f, 0.01f, 1000.0f);
        ImGui::DragFloat("Friction", &rb.Friction, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Restitution", &rb.Restitution, 0.01f, 0.0f, 1.0f);
        ImGui::Checkbox("Use Gravity", &rb.UseGravity);
        ImGui::Checkbox("Is Kinematic", &rb.IsKinematic);
        
        if (rb.IsKinematic) {
            ImGui::Separator();
            ImGui::Text("Kinematic Properties:");
            ImGui::DragFloat3("Linear Velocity", &rb.LinearVelocity.x, 0.1f);
            ImGui::DragFloat3("Angular Velocity", &rb.AngularVelocity.x, 0.1f);
        }
    });

    registry.Register<StaticBodyComponent>("StaticBody", [](StaticBodyComponent& sb) {
        ImGui::DragFloat("Friction", &sb.Friction, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Restitution", &sb.Restitution, 0.01f, 0.0f, 1.0f);
    });
}
