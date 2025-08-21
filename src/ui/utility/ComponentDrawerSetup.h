#pragma once
#include "ComponentDrawerRegistry.h"
#include "ecs/Components.h"
#include "ecs/AnimationComponents.h" // adjust path to where TransformComponent etc. live
#include "rendering/TextureLoader.h"
#include "rendering/Renderer.h"
#include "particles/SpriteLoader.h"
#include "editor/EnginePaths.h"
#include "physics/Physics.h"
#include "pipeline/AssetReference.h" // for ClaymoreGUID

#include <imgui.h>
#include <glm/glm.hpp>
#include <filesystem>
#include <algorithm>
#include <particles/ParticleSystem.h>
#include "animation/AnimationPlayerComponent.h"
// Needed for LoadAnimationAsset and AnimationAsset serialization IO
#include "animation/AnimationSerializer.h"
#include <cstring>
#include <editor/Project.h>
#include "pipeline/AssetLibrary.h"

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

        // Blend mode control
        int blend = (int)e.Uniforms.m_blendMode;
        const char* blendModes[] = { "Alpha", "Additive", "Multiply" };
        if (ImGui::Combo("Blend Mode", &blend, blendModes, IM_ARRAYSIZE(blendModes)))
        {
            e.Uniforms.m_blendMode = (uint32_t)blend;
        }

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
                        e.SpritePath = path;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Default sprite dropdown sourced from claymore/assets/particles/
        // Only show if user hasn't selected a sprite via drag-drop
        {
            static int selectedDefault = 0;
            static std::vector<std::filesystem::path> particlePaths;
            static std::vector<std::string> particleNames;
            if (particlePaths.empty())
            {
                std::filesystem::path exeAssets = EnginePaths::GetEngineAssetPath();
                std::filesystem::path particlesDir = exeAssets / "particles";
                if (std::filesystem::exists(particlesDir))
                {
                    for (auto& entry : std::filesystem::directory_iterator(particlesDir))
                    {
                        if (!entry.is_regular_file()) continue;
                        auto ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga")
                        {
                            particlePaths.push_back(entry.path());
                            particleNames.push_back(entry.path().filename().string());
                        }
                    }
                }
            }

            if (!particleNames.empty())
            {
                if (!e.SpritePath.empty())
                {
                    // Try to sync selectedDefault with current sprite
                    for (size_t i = 0; i < particlePaths.size(); ++i)
                    {
                        if (std::filesystem::path(e.SpritePath).filename() == particlePaths[i].filename())
                        { selectedDefault = (int)i; break; }
                    }
                }
                if (ImGui::Combo("Default Sprite", &selectedDefault, [](void* data, int idx, const char** out_text){
                        auto& names = *reinterpret_cast<std::vector<std::string>*>(data);
                        if (idx < 0 || idx >= (int)names.size()) return false;
                        *out_text = names[idx].c_str(); return true;
                    }, &particleNames, (int)particleNames.size()))
                {
                    // Load the chosen default sprite
                    auto path = particlePaths[(size_t)selectedDefault].string();
                    auto sprite = particles::LoadSprite(path);
                    if (ps::isValid(sprite))
                    {
                        e.SpriteHandle = sprite;
                        e.Uniforms.m_handle = sprite;
                        e.SpritePath = path;
                    }
                }
            }
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

    // AnimationPlayer (Animator) drawer
    registry.Register<cm::animation::AnimationPlayerComponent>("Animator", [](cm::animation::AnimationPlayerComponent& ap) {
        // Ensure at least one state exists
        if (ap.ActiveStates.empty()) ap.ActiveStates.push_back({});

        // Mode
        int mode = (ap.AnimatorMode == cm::animation::AnimationPlayerComponent::Mode::ControllerAnimated) ? 0 : 1;
        ImGui::Combo("Mode", &mode, "Controller Animated\0Animation Player Animated\0");
        ap.AnimatorMode = (mode == 0) ? cm::animation::AnimationPlayerComponent::Mode::ControllerAnimated
                                      : cm::animation::AnimationPlayerComponent::Mode::AnimationPlayerAnimated;

        ImGui::DragFloat("Playback Speed", &ap.PlaybackSpeed, 0.01f, 0.0f, 5.0f);

        // Loop flag applies to the first active state (single-clip)
        bool loop = ap.ActiveStates.front().Loop;
        if (ImGui::Checkbox("Loop", &loop)) {
            ap.ActiveStates.front().Loop = loop;
        }

        // Animation Player mode controls
        if (ap.AnimatorMode == cm::animation::AnimationPlayerComponent::Mode::AnimationPlayerAnimated) {
            ImGui::Separator();
            ImGui::TextDisabled("Animation Player");
            // Single clip path text + choose from registry
            if (!ap.SingleClipPath.empty()) {
                ImGui::Text("Clip: %s", ap.SingleClipPath.c_str());
            } else {
                ImGui::TextDisabled("Clip: (None)");
            }
            ImGui::Checkbox("Play on Start", &ap.PlayOnStart);
            ImGui::Checkbox("Playing", &ap.IsPlaying);

            // Registered animations dropdown (from project assets)
            static int selectedIndex = -1;
            struct AnimOption { std::string name; std::string path; };
            static std::vector<AnimOption> s_options;
            s_options.clear();
            auto root = Project::GetAssetDirectory();
            if (root.empty()) root = std::filesystem::path("assets");
            if (std::filesystem::exists(root)) {
                for (auto& p : std::filesystem::recursive_directory_iterator(root)) {
                    if (!p.is_regular_file()) continue;
                    auto ext = p.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".anim") {
                        AnimOption opt{ p.path().stem().string(), p.path().string() };
                        s_options.push_back(std::move(opt));
                    }
                }
            }
            if (selectedIndex >= (int)s_options.size()) selectedIndex = -1;
            const char* currentLabel = (selectedIndex >= 0 ? s_options[selectedIndex].name.c_str() : "<Select Clip>");
            if (ImGui::BeginCombo("##AnimDropdown", currentLabel)) {
                for (int i = 0; i < (int)s_options.size(); ++i) {
                    bool isSelected = (i == selectedIndex);
                    if (ImGui::Selectable(s_options[i].name.c_str(), isSelected)) {
                        selectedIndex = i;
                        // Load to validate this clip has skeletal content; if not, ignore selection
                        cm::animation::AnimationAsset asset = cm::animation::LoadAnimationAsset(s_options[i].path);
                        bool hasSkeletal = false;
                        for (const auto& t : asset.tracks) {
                            if (!t) continue;
                            if (t->type == cm::animation::TrackType::Bone || t->type == cm::animation::TrackType::Avatar) { hasSkeletal = true; break; }
                        }
                        if (!hasSkeletal) {
                            // Fallback: legacy skeletal
                            cm::animation::AnimationClip legacy = cm::animation::LoadAnimationClip(s_options[i].path);
                            if (!legacy.BoneTracks.empty() || !legacy.HumanoidTracks.empty()) {
                                asset = cm::animation::WrapLegacyClipAsAsset(legacy);
                                hasSkeletal = true;
                            }
                        }
                        if (hasSkeletal) {
                            ap.SingleClipPath = s_options[i].path;
                            ap._InitApplied = false; // allow PlayOnStart to apply on next run
                            // Immediately bind/cached like previous behavior
                            auto assetPtr = std::make_shared<cm::animation::AnimationAsset>(std::move(asset));
                            ap.CachedAssets[0] = assetPtr;
                            if (ap.ActiveStates.empty()) ap.ActiveStates.push_back({});
                            ap.ActiveStates.front().Asset = assetPtr.get();
                            ap.ActiveStates.front().LegacyClip = nullptr;
                            ap.AnimatorMode = cm::animation::AnimationPlayerComponent::Mode::AnimationPlayerAnimated;
                            ap.Controller.reset();
                            ap.CurrentStateId = -1;
                            ap.Debug_CurrentAnimationName = s_options[i].name;
                        } else {
                            // Keep old selection; surface a hint in-place
                            ap.Debug_CurrentAnimationName = std::string("(Non-skeletal) ") + s_options[i].name;
                        }
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        // Debug info
        if (!ap.Debug_CurrentAnimationName.empty()) {
            ImGui::Text("Now Playing: %s", ap.Debug_CurrentAnimationName.c_str());
        }
        if (ap.AnimatorMode == cm::animation::AnimationPlayerComponent::Mode::ControllerAnimated) {
            if (!ap.Debug_CurrentControllerStateName.empty()) {
                ImGui::Text("Controller State: %s", ap.Debug_CurrentControllerStateName.c_str());
            }
            ImGui::Text("Playing: yes");
            // Live controller parameter view
            if (ap.Controller) {
                ImGui::Separator();
                ImGui::TextDisabled("Parameters");
                auto& bb = ap.AnimatorInstance.Blackboard();
                for (const auto& p : ap.Controller->Parameters) {
                    switch (p.Type) {
                        case cm::animation::AnimatorParamType::Bool: {
                            bool v = false; auto it = bb.Bools.find(p.Name); if (it != bb.Bools.end()) v = it->second;
                            ImGui::Text("%s = %s", p.Name.c_str(), v ? "true" : "false");
                        } break;
                        case cm::animation::AnimatorParamType::Int: {
                            int v = 0; auto it = bb.Ints.find(p.Name); if (it != bb.Ints.end()) v = it->second;
                            ImGui::Text("%s = %d", p.Name.c_str(), v);
                        } break;
                        case cm::animation::AnimatorParamType::Float: {
                            float v = 0.0f; auto it = bb.Floats.find(p.Name); if (it != bb.Floats.end()) v = it->second;
                            ImGui::Text("%s = %.3f", p.Name.c_str(), v);
                        } break;
                        case cm::animation::AnimatorParamType::Trigger: {
                            bool v = false; auto it = bb.Triggers.find(p.Name); if (it != bb.Triggers.end()) v = it->second;
                            ImGui::Text("%s (trigger) = %s", p.Name.c_str(), v ? "set" : "unset");
                        } break;
                    }
                }

                // Live transition diagnostics from current state
                ImGui::Separator();
                const int curId = ap.CurrentStateId;
                const cm::animation::AnimatorState* curSt = ap.Controller->FindState(curId);
                if (curSt) {
                    ImGui::TextDisabled("Transitions from '%s' (id=%d)", curSt->Name.c_str(), curId);
                    auto evalCond = [&](const cm::animation::AnimatorCondition& c)->bool{
                        using cm::animation::ConditionMode;
                        switch (c.Mode) {
                            case ConditionMode::If: { auto it=bb.Bools.find(c.Parameter); return it!=bb.Bools.end() && it->second; }
                            case ConditionMode::IfNot: { auto it=bb.Bools.find(c.Parameter); return it!=bb.Bools.end() && !it->second; }
                            case ConditionMode::Greater: {
                                auto itf=bb.Floats.find(c.Parameter); if(itf!=bb.Floats.end()) return itf->second>c.Threshold;
                                auto iti=bb.Ints.find(c.Parameter);   if(iti!=bb.Ints.end())   return iti->second>c.IntThreshold; return false; }
                            case ConditionMode::Less: {
                                auto itf=bb.Floats.find(c.Parameter); if(itf!=bb.Floats.end()) return itf->second<c.Threshold;
                                auto iti=bb.Ints.find(c.Parameter);   if(iti!=bb.Ints.end())   return iti->second<c.IntThreshold; return false; }
                            case ConditionMode::Equals: {
                                auto itf=bb.Floats.find(c.Parameter); if(itf!=bb.Floats.end()) return itf->second==c.Threshold;
                                auto iti=bb.Ints.find(c.Parameter);   if(iti!=bb.Ints.end())   return iti->second==c.IntThreshold; return false; }
                            case ConditionMode::NotEquals: {
                                auto itf=bb.Floats.find(c.Parameter); if(itf!=bb.Floats.end()) return itf->second!=c.Threshold;
                                auto iti=bb.Ints.find(c.Parameter);   if(iti!=bb.Ints.end())   return iti->second!=c.IntThreshold; return false; }
                            case ConditionMode::Trigger: { auto it=bb.Triggers.find(c.Parameter); return it!=bb.Triggers.end() && it->second; }
                        }
                        return false;
                    };
                    for (const auto& tr : ap.Controller->Transitions) {
                        if (tr.FromState != curId) continue;
                        bool ok = true; for (const auto& c : tr.Conditions) { if (!evalCond(c)) { ok=false; break; } }
                        const auto* toSt = ap.Controller->FindState(tr.ToState);
                        ImGui::Text("-> %s (id=%d): %s", toSt?toSt->Name.c_str():"?", tr.ToState, ok?"match":"no match");
                    }
                }
            }
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

    // Text Renderer
    registry.Register<TextRendererComponent>("TextRenderer", [](TextRendererComponent& t) {
        // Text field
        static char buffer[1024];
        strncpy(buffer, t.Text.c_str(), sizeof(buffer)-1);
        buffer[sizeof(buffer)-1] = '\0';
        if (ImGui::InputTextMultiline("Text", buffer, sizeof(buffer), ImVec2(-1, 80))) {
            t.Text = buffer;
        }

        // Size and color
        ImGui::DragFloat("Pixel Size", &t.PixelSize, 1.0f, 6.0f, 256.0f);

        // Convert ABGR to ImGui RGBA for UI editing
        ImVec4 col = ImGui::ColorConvertU32ToFloat4(t.ColorAbgr);
        if (ImGui::ColorEdit4("Color", &col.x)) {
            t.ColorAbgr = ImGui::ColorConvertFloat4ToU32(col);
        }

        ImGui::Checkbox("World Space", &t.WorldSpace);
        if (!t.WorldSpace) {
            ImGui::Separator();
            ImGui::TextDisabled("UI Anchoring");
            ImGui::Checkbox("Use Anchor", &t.AnchorEnabled);
            if (t.AnchorEnabled) {
                const char* anchors[] = { "TopLeft","Top","TopRight","Left","Center","Right","BottomLeft","Bottom","BottomRight" };
                int a = (int)t.Anchor;
                if (ImGui::Combo("Anchor", &a, anchors, IM_ARRAYSIZE(anchors))) t.Anchor = (UIAnchorPreset)a;
                ImGui::DragFloat2("Offset", &t.AnchorOffset.x, 1.0f);
            } else {
                ImGui::Text("Screen Position = Transform.Position.xy");
            }
        }
    });

    // Canvas drawer
    registry.Register<CanvasComponent>("Canvas", [](CanvasComponent& c){
        int space = (int)c.Space; const char* spaces[] = { "ScreenSpace", "WorldSpace" };
        ImGui::Combo("Space", &space, spaces, IM_ARRAYSIZE(spaces)); c.Space = (CanvasComponent::RenderSpace)space;
        ImGui::DragInt("Width", &c.Width, 1, 0, 16384);
        ImGui::DragInt("Height", &c.Height, 1, 0, 16384);
        ImGui::DragFloat("DPI Scale", &c.DPIScale, 0.01f, 0.25f, 4.0f);
        ImGui::DragInt("Sort Order", &c.SortOrder, 1, -1000, 1000);
        ImGui::Checkbox("Block Scene Input", &c.BlockSceneInput);
    });

    // Panel drawer
    registry.Register<PanelComponent>("Panel", [](PanelComponent& p){
        // Scope IDs by component address to avoid collisions with Transform controls (e.g., Scale)
        ImGui::PushID(&p);
        ImGui::Checkbox("Visible", &p.Visible);
        ImGui::DragFloat2("Size", &p.Size.x, 1.0f, 0.0f, 10000.0f);
        ImGui::DragFloat2("Scale", &p.Scale.x, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Rotation", &p.Rotation, 0.1f, -360.0f, 360.0f);
        ImGui::DragInt("Z Order", &p.ZOrder, 1, -1000, 1000);
        ImGui::Separator();
        ImGui::TextDisabled("Texture");
        if (p.Texture.IsValid()) {
            if (auto* entry = AssetLibrary::Instance().GetAsset(p.Texture)) {
                // Ensure texture is loaded for preview
                if (!entry->texture || !bgfx::isValid(*entry->texture)) {
                    auto tex = AssetLibrary::Instance().LoadTexture(p.Texture);
                    (void)tex;
                }
                ImTextureID thumb = (entry->texture && bgfx::isValid(*entry->texture)) ? TextureLoader::ToImGuiTextureID(*entry->texture) : 0;
                ImGui::Image(thumb, ImVec2(64,64));
            } else {
                ImGui::TextDisabled("(No loaded texture)");
            }
        } else {
            ImGui::TextDisabled("(None)");
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
                const char* path = (const char*)payload->Data;
                if (path) {
                    std::string ext = std::filesystem::path(path).extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
                        AssetEntry* entry = AssetLibrary::Instance().GetAsset(std::string(path));
                        if (!entry) {
                            ClaymoreGUID guid = AssetLibrary::Instance().GetGUIDForPath(path);
                            if (guid.high == 0 && guid.low == 0) {
                                guid = ClaymoreGUID::Generate();
                            }
                            AssetLibrary::Instance().RegisterAsset(AssetReference(guid, 0, (int)AssetType::Texture), AssetType::Texture, path, std::filesystem::path(path).filename().string());
                            entry = AssetLibrary::Instance().GetAsset(path);
                        }
                        if (entry) {
                            p.Texture = entry->reference;
                            // Preload so renderer can use immediately and preview shows correctly
                            AssetLibrary::Instance().LoadTexture(p.Texture);
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::Separator();
        ImGui::TextDisabled("Anchoring");
        ImGui::Checkbox("Use Anchor", &p.AnchorEnabled);
        if (p.AnchorEnabled) {
            const char* anchors[] = { "TopLeft","Top","TopRight","Left","Center","Right","BottomLeft","Bottom","BottomRight" };
            int a = (int)p.Anchor;
            if (ImGui::Combo("Anchor", &a, anchors, IM_ARRAYSIZE(anchors))) p.Anchor = (UIAnchorPreset)a;
            ImGui::DragFloat2("Offset", &p.AnchorOffset.x, 1.0f);
        } else {
            ImGui::DragFloat2("Position", &p.Position.x, 1.0f);
            ImGui::DragFloat2("Pivot", &p.Pivot.x, 0.01f, 0.0f, 1.0f);
        }
        ImGui::ColorEdit4("Tint", &p.TintColor.x);
        ImGui::DragFloat4("UV Rect", &p.UVRect.x, 0.001f, 0.0f, 1.0f);
        // Fill mode & theming
        const char* modes[] = { "Stretch", "Tile", "NineSlice" };
        int m = (int)p.Mode;
        if (ImGui::Combo("Fill Mode", &m, modes, IM_ARRAYSIZE(modes))) p.Mode = (PanelComponent::FillMode)m;
        if (p.Mode == PanelComponent::FillMode::Tile) {
            ImGui::DragFloat2("Tile Repeat", &p.TileRepeat.x, 0.01f, 0.01f, 1000.0f);
        }
        if (p.Mode == PanelComponent::FillMode::NineSlice) {
            ImGui::DragFloat4("Slice UV (L T R B)", &p.SliceUV.x, 0.001f, 0.0f, 0.5f);
        }
        ImGui::PopID();
    });

    // Button drawer
    registry.Register<ButtonComponent>("Button", [](ButtonComponent& b){
        ImGui::Checkbox("Interactable", &b.Interactable);
        ImGui::Checkbox("Toggle", &b.Toggle);
        ImGui::Checkbox("Toggled", &b.Toggled);
        ImGui::ColorEdit4("Normal Tint", &b.NormalTint.x);
        ImGui::ColorEdit4("Hover Tint", &b.HoverTint.x);
        ImGui::ColorEdit4("Pressed Tint", &b.PressedTint.x);
        // Live state read-only
        ImGui::Separator();
        ImGui::TextDisabled("Runtime State (read-only)");
        ImGui::Text("Hovered: %s", b.Hovered ? "true" : "false");
        ImGui::Text("Pressed: %s", b.Pressed ? "true" : "false");
        ImGui::Text("Clicked: %s", b.Clicked ? "true" : "false");
    });
}
