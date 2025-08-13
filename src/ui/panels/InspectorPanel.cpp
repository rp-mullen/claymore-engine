#include "InspectorPanel.h"
#include "../utility/ComponentDrawerRegistry.h"
#include <glm/glm.hpp>
#include <imgui.h>
#include <string>
#include "scripting/ScriptSystem.h"
#include "scripting/ManagedScriptComponent.h"
#include "scripting/ScriptReflectionInterop.h"
#include "scripting/ScriptReflection.h"
#include "ecs/EntityData.h"
#include "ecs/ComponentUtils.h"
#include "rendering/PBRMaterial.h"
#include "rendering/TextureLoader.h"
#include <bgfx/bgfx.h>
#include "animation/AnimatorController.h"
#include "animation/AnimationPlayerComponent.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include "ui/panels/AvatarBuilderPanel.h"
// For project asset directory discovery and animation asset helpers
#include <editor/Project.h>
#include "animation/AnimationSerializer.h"

bool DrawVec3Control(const char* label, glm::vec3& values, float resetValue = 0.0f) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 80.0f);
    ImGui::Text(label);
    ImGui::NextColumn();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 4, 0 });

    float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
    ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
    float columnWidth = (ImGui::GetContentRegionAvail().x - 3 * buttonSize.x) / 3.0f;

    auto DrawAxis = [&](const char* axisLabel, float& v, const ImVec4& color) {
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        if (ImGui::Button(axisLabel, buttonSize)) {
            v = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(columnWidth);
        changed |= ImGui::DragFloat(("##" + std::string(axisLabel)).c_str(), &v, 0.1f);
        ImGui::SameLine();
        };

    DrawAxis("X", values.x, { 0.8f, 0.1f, 0.15f, 1.0f });
    DrawAxis("Y", values.y, { 0.2f, 0.7f, 0.2f, 1.0f });
    DrawAxis("Z", values.z, { 0.1f, 0.25f, 0.8f, 1.0f });

    ImGui::PopStyleVar();
    ImGui::Columns(1);
    ImGui::PopID();
    return changed;
}

void InspectorPanel::OnImGuiRender() {
    ImGui::Begin("Inspector");

    // Timeline key inspection removed; the new animation panel owns track/key inspection UI

    // Prefer entity selection if available; otherwise, show animator binding when set
    if (!(m_SelectedEntity && *m_SelectedEntity != -1 && m_Context)
        && m_HasAnimatorBinding && m_AnimatorBinding.Name) {
        std::string dummyName = *m_AnimatorBinding.Name;
        ShowAnimatorStateProperties(dummyName,
                                    *m_AnimatorBinding.ClipPath,
                                    *m_AnimatorBinding.Speed,
                                    *m_AnimatorBinding.Loop,
                                    m_AnimatorBinding.IsDefault,
                                    m_AnimatorBinding.MakeDefault);
        ImGui::End();
        return;
    }

    if (m_SelectedEntity && *m_SelectedEntity != -1 && m_Context) {
        DrawComponents(*m_SelectedEntity);
        // Offer to add an Animator if a skeleton exists but no AnimationPlayer is attached
        auto* data = m_Context->GetEntityData(*m_SelectedEntity);
        if (data && data->Skeleton && !data->AnimationPlayer) {
            ImGui::Separator();
            if (ImGui::Button("Add Animator to Entity")) {
                data->AnimationPlayer = new cm::animation::AnimationPlayerComponent();
            }
        }
    }
    else {
        ImGui::Text("No entity selected.");
    }

    ImGui::End();
}

// Timeline key inspector removed (new animation panel owns it)

void InspectorPanel::ShowAnimatorStateProperties(const std::string& stateName,
                                    std::string& clipPath,
                                    float& speed,
                                    bool& loop,
                                    bool isDefault,
                                    std::function<void()> onMakeDefault,
                                    std::vector<std::pair<std::string, int>>* conditionsInt,
                                    std::vector<std::tuple<std::string, int, float>>* conditionsFloat)
{
    ImGui::Separator();
    ImGui::Text("Animator State: %s", stateName.c_str());
    if (isDefault) ImGui::TextDisabled("(Default Entry)");
    else if (ImGui::Button("Make Default")) { if (onMakeDefault) onMakeDefault(); }

    // Registered animations dropdown (project-wide .anim files)
    {
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
                    s_options.push_back({ p.path().stem().string(), p.path().string() });
                }
            }
        }

        // Try to sync selection with current clip path
        selectedIndex = -1;
        for (int i = 0; i < (int)s_options.size(); ++i) {
            if (s_options[i].path == clipPath) { selectedIndex = i; break; }
        }

        const char* currentLabel = (selectedIndex >= 0 ? s_options[selectedIndex].name.c_str() : "<Select Clip>");
        if (ImGui::BeginCombo("Clip", currentLabel)) {
            for (int i = 0; i < (int)s_options.size(); ++i) {
                bool isSel = (i == selectedIndex);
                if (ImGui::Selectable(s_options[i].name.c_str(), isSel)) {
                    selectedIndex = i;
                    // Prefer unified .anim asset when available (same file here)
                    clipPath = s_options[i].path;
                    if (m_HasAnimatorBinding && m_AnimatorBinding.AssetPath) {
                        *m_AnimatorBinding.AssetPath = s_options[i].path;
                    }
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    // Drag-and-drop animation file onto clip path (legacy/manual)
    char buf[260];
    strncpy(buf, clipPath.c_str(), sizeof(buf)); buf[sizeof(buf)-1] = 0;
    ImGui::InputText("Clip Path", buf, sizeof(buf));
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
            const char* path = (const char*)payload->Data;
            // Accept only .anim
            if (path && strstr(path, ".anim")) {
                strncpy(buf, path, sizeof(buf)); buf[sizeof(buf)-1] = 0;
            }
        }
        ImGui::EndDragDropTarget();
    }
    clipPath = buf;

    // Unified asset path next to legacy for migration
    if (m_HasAnimatorBinding && m_AnimatorBinding.AssetPath) {
        char abuf[260];
        strncpy(abuf, m_AnimatorBinding.AssetPath->c_str(), sizeof(abuf)); abuf[sizeof(abuf)-1] = 0;
        ImGui::InputText("Asset Path", abuf, sizeof(abuf));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
                const char* path = (const char*)payload->Data;
                if (path && strstr(path, ".anim")) {
                    strncpy(abuf, path, sizeof(abuf)); abuf[sizeof(abuf)-1] = 0;
                }
            }
            ImGui::EndDragDropTarget();
        }
        *m_AnimatorBinding.AssetPath = abuf;
    }

    ImGui::DragFloat("Speed", &speed, 0.01f, 0.0f, 10.0f);
    ImGui::Checkbox("Loop", &loop);

    // Optional future: render conditions
}

void InspectorPanel::DrawComponents(EntityID entity) {
    auto* data = m_Context->GetEntityData(entity);
    if (!data) return;

    ImGui::Text("Entity: %s", data->Name.c_str());
    ImGui::Separator();

    auto& registry = ComponentDrawerRegistry::Instance();

    if (&data->Transform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        registry.DrawComponentUI("Transform", &data->Transform);
        // Timeline key operations removed
    }

    if (data->Mesh && ImGui::CollapsingHeader("Mesh")) {
        registry.DrawComponentUI("Mesh", data->Mesh);

        // Unique material toggle
                bool unique = data->Mesh->UniqueMaterial;
        if (ImGui::Checkbox("Unique Material", &unique)) {
            if (unique && !data->Mesh->UniqueMaterial) {
                // Make a shallow copy of the material so this entity can have unique overrides
                if (data->Mesh->material) {
                    // NOTE: Assumes PBRMaterial for now; deep copy constructor
                    auto base = data->Mesh->material;
                    // Attempt to clone by copy construction if derived from Material
                    // If not copyable, fall back to same pointer.
                    std::shared_ptr<Material> clone;
                    if (auto pbr = std::dynamic_pointer_cast<PBRMaterial>(base)) {
                        clone = std::make_shared<PBRMaterial>(*pbr);
                    } else {
                        // Fallback: use the same shared instance (no unique copy possible)
                        clone = base;
                    }
                    data->Mesh->material = clone;
                }
            }
            data->Mesh->UniqueMaterial = unique;
        }

        // Property overrides (MaterialPropertyBlock)
        if (!data->Mesh->UniqueMaterial) {
            ImGui::Separator();
            ImGui::TextDisabled("Material Overrides (Property Block)");
            // Color tint as example
            glm::vec4 tint(1.0f);
            auto itTint = data->Mesh->PropertyBlock.Vec4Uniforms.find("u_ColorTint");
            if (itTint != data->Mesh->PropertyBlock.Vec4Uniforms.end())
                tint = itTint->second;
            if (ImGui::ColorEdit4("Tint", &tint.x)) {
                data->Mesh->PropertyBlock.Vec4Uniforms["u_ColorTint"] = tint;
            }

            // Albedo texture override via drag-drop
            bgfx::TextureHandle overrideTex = BGFX_INVALID_HANDLE;
            auto itTex = data->Mesh->PropertyBlock.Textures.find("u_AlbedoSampler");
            if (itTex != data->Mesh->PropertyBlock.Textures.end())
                overrideTex = itTex->second;

            ImGui::Text("Albedo Texture Override:");
            if (bgfx::isValid(overrideTex)) {
                ImGui::ImageButton("OverrideTex", (ImTextureID)(uintptr_t)overrideTex.idx, ImVec2(64,64));
            } else {
                ImGui::Button("Drop texture", ImVec2(64,64));
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
                    const char* path = (const char*)payload->Data;
                    bgfx::TextureHandle tex = TextureLoader::Load2D(path);
                    if (bgfx::isValid(tex)) {
                        data->Mesh->PropertyBlock.Textures["u_AlbedoSampler"] = tex;
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Remove Mesh Component", ImVec2(-1, 0))) {
            if (data->Mesh) {
                // Only release references; do not attempt to destroy shared GPU buffers here
                data->Mesh->mesh = nullptr;
                data->Mesh->material.reset();
                delete data->Mesh;
                data->Mesh = nullptr;
            }
        }
        ImGui::PopStyleColor(2);
    }

    if (data->Light && ImGui::CollapsingHeader("Light")) {
        registry.DrawComponentUI("Light", data->Light);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Remove Light Component", ImVec2(-1, 0))) {
            delete data->Light;
            data->Light = nullptr;
        }
        ImGui::PopStyleColor(2);
    }

    if (data->Collider && ImGui::CollapsingHeader("Collider")) {
        registry.DrawComponentUI("Collider", data->Collider);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Remove Collider Component", ImVec2(-1, 0))) {
            delete data->Collider;
            data->Collider = nullptr;
        }
        ImGui::PopStyleColor(2);
    }

    if (data->Camera && ImGui::CollapsingHeader("Camera")) {
        registry.DrawComponentUI("Camera", data->Camera);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Remove Camera Component", ImVec2(-1, 0))) {
            delete data->Camera;
            data->Camera = nullptr;
        }
        ImGui::PopStyleColor(2);
    }

    if (data->RigidBody && ImGui::CollapsingHeader("RigidBody")) {
        registry.DrawComponentUI("RigidBody", data->RigidBody);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Remove RigidBody Component", ImVec2(-1, 0))) {
            delete data->RigidBody;
            data->RigidBody = nullptr;
        }
        ImGui::PopStyleColor(2);
    }

    if (data->StaticBody && ImGui::CollapsingHeader("StaticBody")) {
        registry.DrawComponentUI("StaticBody", data->StaticBody);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Remove StaticBody Component", ImVec2(-1, 0))) {
            delete data->StaticBody;
            data->StaticBody = nullptr;
        }
        ImGui::PopStyleColor(2);
    }

    if (data->Terrain && ImGui::CollapsingHeader("Terrain")) {
        registry.DrawComponentUI("Terrain", data->Terrain);
    }

    // Particle System
    if (data->Emitter && ImGui::CollapsingHeader("Particle Emitter")) {
        registry.DrawComponentUI("ParticleEmitter", data->Emitter);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Remove Particle System Component", ImVec2(-1, 0))) {
            delete data->Emitter;
            data->Emitter = nullptr;
        }
        ImGui::PopStyleColor(2);
    }

    // Text Renderer
    if (data->Text && ImGui::CollapsingHeader("TextRenderer")) {
        registry.DrawComponentUI("TextRenderer", data->Text);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Remove TextRenderer Component", ImVec2(-1, 0))) {
            delete data->Text;
            data->Text = nullptr;
        }
        ImGui::PopStyleColor(2);
    }

    // Draw script components
    for (size_t i = 0; i < data->Scripts.size(); ++i) {
        DrawScriptComponent(data->Scripts[i], static_cast<int>(i), entity);
    }

    // Skeleton tools (visible whether or not Animator exists)
    if (data->Skeleton && ImGui::CollapsingHeader("Skeleton")) {
        ImGui::Text("Bones: %d", (int)data->Skeleton->BoneNameToIndex.size());
        if (ImGui::Button("Open Avatar Builder")) {
            if (m_AvatarBuilder) m_AvatarBuilder->OpenForEntity(entity);
        }
    }

    if (data->AnimationPlayer && ImGui::CollapsingHeader("Animator")) {
        // Draw component UI (includes mode and single-clip controls)
        registry.DrawComponentUI("Animator", data->AnimationPlayer);

        // Controller controls visible only when in ControllerAnimated mode
        if (data->AnimationPlayer->AnimatorMode == cm::animation::AnimationPlayerComponent::Mode::ControllerAnimated) {
            ImGui::Separator();
            ImGui::TextDisabled("Controller (optional)");
            ImGui::Text("Controller: %s", data->AnimationPlayer->ControllerPath.c_str());
            // Registered controller dropdown (search .animctrl under assets)
            {
                static int selectedCtrl = -1;
                struct COpt { std::string name; std::string path; };
                static std::vector<COpt> s_ctrls;
                s_ctrls.clear();
                auto root = Project::GetAssetDirectory();
                if (root.empty()) root = std::filesystem::path("assets");
                if (std::filesystem::exists(root)) {
                    for (auto& p : std::filesystem::recursive_directory_iterator(root)) {
                        if (!p.is_regular_file()) continue;
                        auto ext = p.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".animctrl") {
                            s_ctrls.push_back({ p.path().stem().string(), p.path().string() });
                        }
                    }
                }
                // Sync selection to current
                selectedCtrl = -1;
                for (int i=0;i<(int)s_ctrls.size();++i) {
                    if (s_ctrls[i].path == data->AnimationPlayer->ControllerPath) { selectedCtrl = i; break; }
                }
                const char* cur = (selectedCtrl >= 0 ? s_ctrls[selectedCtrl].name.c_str() : "<Select Controller>");
                if (ImGui::BeginCombo("##CtrlDropdown", cur)) {
                    for (int i=0;i<(int)s_ctrls.size();++i) {
                        bool sel = (i==selectedCtrl);
                        if (ImGui::Selectable(s_ctrls[i].name.c_str(), sel)) {
                            selectedCtrl = i;
                            data->AnimationPlayer->ControllerPath = s_ctrls[i].path;
                        }
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Set Path")) {
                // For MVP, read from clipboard
                if (const char* clip = ImGui::GetClipboardText()) {
                    data->AnimationPlayer->ControllerPath = clip;
                }
            }
            if (ImGui::Button("Load Controller")) {
                // Load JSON controller file
                std::ifstream in(data->AnimationPlayer->ControllerPath);
                if (in) {
                    nlohmann::json j; in >> j;
                    auto ctrl = std::make_shared<cm::animation::AnimatorController>();
                    nlohmann::from_json(j, *ctrl);
                    data->AnimationPlayer->Controller = ctrl;
                    data->AnimationPlayer->AnimatorInstance.SetController(ctrl);
                    data->AnimationPlayer->AnimatorInstance.ResetToDefaults();
                    data->AnimationPlayer->CurrentStateId = ctrl->DefaultState;
                }
            }
        }
    }

    ImGui::Separator();
    DrawAddComponentButton(entity);
}

void InspectorPanel::DrawAddComponentButton(EntityID entity) {
    if (ImGui::Button("Add Component", ImVec2(-1, 0))) {
        m_ShowAddComponentPopup = true;
    }

    if (m_ShowAddComponentPopup) {
        ImGui::OpenPopup("Add Component");
        m_ShowAddComponentPopup = false;
    }

    if (ImGui::BeginPopup("Add Component")) {
        auto* data = m_Context->GetEntityData(entity);
        if (!data) {
            ImGui::EndPopup();
            return;
        }

        ImGui::Text("Native Components:");
        ImGui::Separator();

        // Native components
        if (!data->Mesh && ImGui::MenuItem("Mesh Component")) {
            data->Mesh = new MeshComponent();
        }

        if (!data->Light && ImGui::MenuItem("Light Component")) {
            data->Light = new LightComponent();
        }

        if (!data->Collider && ImGui::MenuItem("Collider Component")) {
            data->Collider = new ColliderComponent();
        }

        if (!data->Camera && ImGui::MenuItem("Camera Component")) {
           data->Camera = new CameraComponent();
           }

        if (!data->RigidBody && !data->StaticBody && ImGui::MenuItem("RigidBody Component")) {
            data->RigidBody = new RigidBodyComponent();
            EnsureCollider(data->RigidBody, data);

            // If the scene is currently playing, create the physics body immediately
            if (m_Context && m_Context->m_IsPlaying && data->Collider) {
                // Update collider size to respect current scale for box shapes
                if (data->Collider->ShapeType == ColliderShape::Box) {
                    data->Collider->Size = glm::abs(data->Collider->Size * data->Transform.Scale);
                }
                // Build the collision shape and create the Jolt body
                data->Collider->BuildShape();
                m_Context->CreatePhysicsBody(entity, data->Transform, *data->Collider);
            }
        }

        if (!data->RigidBody && !data->StaticBody && ImGui::MenuItem("StaticBody Component")) {
            data->StaticBody = new StaticBodyComponent();
            EnsureCollider(data->StaticBody, data);

            // If the scene is currently playing, create the physics body immediately
            if (m_Context && m_Context->m_IsPlaying && data->Collider) {
                if (data->Collider->ShapeType == ColliderShape::Box) {
                    data->Collider->Size = glm::abs(data->Collider->Size * data->Transform.Scale);
                }
                data->Collider->BuildShape();
                m_Context->CreatePhysicsBody(entity, data->Transform, *data->Collider);
            }
        }

        if (!data->Emitter && ImGui::MenuItem("Particle Emitter Component")) {
            data->Emitter = new ParticleEmitterComponent();
        }

        if (!data->Text && ImGui::MenuItem("TextRenderer Component")) {
            data->Text = new TextRendererComponent();
        }

        ImGui::Separator();
        ImGui::Text("Script Components:");
        ImGui::Separator();

        // Script components
        for (const auto& scriptName : g_RegisteredScriptNames) {
            // Check if this script is already attached
            bool alreadyAttached = false;
            for (const auto& script : data->Scripts) {
                if (script.ClassName == scriptName) {
                    alreadyAttached = true;
                    break;
                }
            }

            if (!alreadyAttached && ImGui::MenuItem(scriptName.c_str())) {
                ScriptInstance instance;
                instance.ClassName = scriptName;
                
                // Create the script instance
                auto created = ScriptSystem::Instance().Create(scriptName);
                if (created) {
                    instance.Instance = created;
                    data->Scripts.push_back(instance);

                    // Only invoke OnCreate immediately when the scene is playing.
                    // In edit mode, OnCreate will run when the scene is cloned for play.
                    if (m_Context && m_Context->m_IsPlaying) {
                        created->OnCreate(Entity(entity, m_Context));
                    }
                } else {
                    std::cerr << "[Inspector] Failed to create script of type '" << scriptName << "'\n";
                }
            }
        }

        ImGui::EndPopup();
    }
}

void InspectorPanel::DrawScriptComponent(const ScriptInstance& script, int index, EntityID entity) {
    std::string headerName = script.ClassName + "##" + std::to_string(index);
    
    if (ImGui::CollapsingHeader(headerName.c_str())) {
        ImGui::PushID(index);
        
        ImGui::Text("Script Type: %s", script.ClassName.c_str());
        
        // Remove button
        if (ImGui::Button("Remove Script")) {
            auto* data = m_Context->GetEntityData(entity);
            if (data && index >= 0 && index < static_cast<int>(data->Scripts.size())) {
                data->Scripts.erase(data->Scripts.begin() + index);
            }
        }
        
        // Draw script properties using reflection
        if (ScriptReflection::HasProperties(script.ClassName)) {
            auto& properties = ScriptReflection::GetScriptProperties(script.ClassName);
            void* scriptHandle = nullptr;
            if(script.Instance && script.Instance->GetBackend() == ScriptBackend::Managed) {
                if(auto managed = std::dynamic_pointer_cast<ManagedScriptComponent>(script.Instance))
                    scriptHandle = managed->GetHandle();
            }
            for (auto& property : properties) {
                DrawScriptProperty(property, scriptHandle);
            }
        } else {
            ImGui::Text("No exposed properties");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                              "Add [SerializeField] attributes to C# properties to expose them here");
        }
        
                 ImGui::PopID();
     }
}

void InspectorPanel::DrawScriptProperty(PropertyInfo& property, void* scriptHandle) {
    ImGui::PushID(property.name.c_str());
    bool updated = false;
    
    switch (property.type) {
        case PropertyType::Int: {
            int value = std::get<int>(property.currentValue);
            if (ImGui::DragInt(property.name.c_str(), &value)) {
                property.currentValue = value;
                if (property.setter) {
                    property.setter(value);
                }
                updated = true;
            }
            break;
        }
        
        case PropertyType::Float: {
            float value = std::get<float>(property.currentValue);
            if (ImGui::DragFloat(property.name.c_str(), &value, 0.1f)) {
                property.currentValue = value;
                if (property.setter) {
                    property.setter(value);
                }
                updated = true;
            }
            break;
        }
        
        case PropertyType::Bool: {
            bool value = std::get<bool>(property.currentValue);
            if (ImGui::Checkbox(property.name.c_str(), &value)) {
                property.currentValue = value;
                if (property.setter) {
                    property.setter(value);
                }
                updated = true;
            }
            break;
        }
        
        case PropertyType::String: {
            std::string value = std::get<std::string>(property.currentValue);
            char buffer[256];
            strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
            
            if (ImGui::InputText(property.name.c_str(), buffer, sizeof(buffer))) {
                property.currentValue = std::string(buffer);
                if (property.setter) {
                    property.setter(std::string(buffer));
                }
                updated = true;
            }
            break;
        }
        
        case PropertyType::Vector3: {
            glm::vec3 value = std::get<glm::vec3>(property.currentValue);
            if (DrawVec3Control(property.name.c_str(), value)) {
                property.currentValue = value;
                if (property.setter) {
                    property.setter(value);
                }
                updated = true;
            }
            break;
        }

        case PropertyType::Entity: {
            int entityId = std::get<int>(property.currentValue);
            const char* btnLabel = "None";
            if(entityId != -1) {
                if(auto* entData = m_Context->GetEntityData(entityId))
                    btnLabel = entData->Name.c_str();
            }
            ImGui::Button(btnLabel, ImVec2(-1,0));
            if(ImGui::BeginDragDropTarget()) {
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
                    EntityID dropped = *(EntityID*)payload->Data;
                    property.currentValue = static_cast<int>(dropped);
                    if(property.setter) property.setter(PropertyValue{ static_cast<int>(dropped) });
                    updated = true;
                }
                ImGui::EndDragDropTarget();
            }
            break;
        }
    }
    if(updated && scriptHandle && SetManagedFieldPtr)
    {
        void* boxed = ScriptReflection::ValueToBox(property.currentValue);
        SetManagedFieldPtr(scriptHandle, property.name.c_str(), boxed);
    }
    ImGui::PopID();
}
