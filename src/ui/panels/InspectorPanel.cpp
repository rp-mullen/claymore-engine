#include "InspectorPanel.h"
#include "../utility/ComponentDrawerRegistry.h"
#include <glm/glm.hpp>
#include <imgui.h>
#include <string>
#include "scripting/ScriptSystem.h"
#include "scripting/ScriptReflection.h"

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

    if (m_SelectedEntity && *m_SelectedEntity != -1 && m_Context) {
        DrawComponents(*m_SelectedEntity);
    }
    else {
        ImGui::Text("No entity selected.");
    }

    ImGui::End();
}

void InspectorPanel::DrawComponents(EntityID entity) {
    auto* data = m_Context->GetEntityData(entity);
    if (!data) return;

    ImGui::Text("Entity: %s", data->Name.c_str());
    ImGui::Separator();

    auto& registry = ComponentDrawerRegistry::Instance();

    if (&data->Transform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        registry.DrawComponentUI("Transform", &data->Transform);
        // We don't call MarkTransformDirty here � it's handled in the drawer!
    }

    if (data->Mesh && ImGui::CollapsingHeader("Mesh")) {
        registry.DrawComponentUI("Mesh", data->Mesh);
    }

    if (data->Light && ImGui::CollapsingHeader("Light")) {
        registry.DrawComponentUI("Light", data->Light);
    }

    if (data->Collider && ImGui::CollapsingHeader("Collider")) {
        registry.DrawComponentUI("Collider", data->Collider);
    }

    // Draw script components
    for (size_t i = 0; i < data->Scripts.size(); ++i) {
        DrawScriptComponent(data->Scripts[i], static_cast<int>(i), entity);
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
                    
                    // Initialize the script with the entity
                    if (created) {
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
            auto properties = ScriptReflection::GetScriptProperties(script.ClassName);
            for (auto& property : properties) {
                DrawScriptProperty(property);
            }
        } else {
            ImGui::Text("No exposed properties");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                              "Add [SerializeField] attributes to C# properties to expose them here");
        }
        
                 ImGui::PopID();
     }
}

void InspectorPanel::DrawScriptProperty(PropertyInfo& property) {
    ImGui::PushID(property.name.c_str());
    
    switch (property.type) {
        case PropertyType::Int: {
            int value = std::get<int>(property.currentValue);
            if (ImGui::DragInt(property.name.c_str(), &value)) {
                property.currentValue = value;
                if (property.setter) {
                    property.setter(value);
                }
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
            }
            break;
        }
    }
    
    ImGui::PopID();
}
