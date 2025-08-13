#include "editor/panels/MaterialInspectorPanel.h"
#include "ui/UILayer.h"
#include "ui/panels/ProjectPanel.h"
#include "rendering/MaterialAsset.h"
#include <imgui.h>
#include <filesystem>
#include <glm/glm.hpp>
#include <cstring>

static void DrawVec4Field(const char* label, glm::vec4& v) {
    ImGui::DragFloat4(label, &v.x, 0.01f);
}

void RenderMaterialInspector(UILayer* uiLayer) {
    if (!uiLayer) return;
    const std::string path = uiLayer->GetProjectPanel().GetSelectedItemPath();
    if (std::filesystem::path(path).extension() != ".mat") return;

    MaterialAssetDesc desc;
    bool loaded = LoadMaterialAsset(path, desc);
    if (!loaded) {
        ImGui::TextDisabled("(invalid material file)");
        return;
    }

    ImGui::TextUnformatted("Material");
    ImGui::Separator();
    char nameBuf[256];
    strncpy(nameBuf, desc.name.c_str(), sizeof(nameBuf)); nameBuf[sizeof(nameBuf)-1]=0;
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) desc.name = nameBuf;

    char vsBuf[256]; strncpy(vsBuf, desc.shaderVS.c_str(), sizeof(vsBuf)); vsBuf[sizeof(vsBuf)-1]=0;
    char fsBuf[256]; strncpy(fsBuf, desc.shaderFS.c_str(), sizeof(fsBuf)); fsBuf[sizeof(fsBuf)-1]=0;
    ImGui::InputText("VS", vsBuf, sizeof(vsBuf));
    ImGui::InputText("FS", fsBuf, sizeof(fsBuf));
    desc.shaderVS = vsBuf; desc.shaderFS = fsBuf;

    // Texture paths with drag-drop targets
    auto texField = [&](const char* label, std::string& p) {
        char buf[512]; strncpy(buf, p.c_str(), sizeof(buf)); buf[sizeof(buf)-1]=0;
        ImGui::InputText(label, buf, sizeof(buf));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
                const char* drop = (const char*)payload->Data;
                if (drop) {
                    std::string ext = std::filesystem::path(drop).extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
                        strncpy(buf, drop, sizeof(buf)); buf[sizeof(buf)-1]=0;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        p = buf;
    };
    texField("Albedo", desc.albedoPath);
    texField("MetallicRoughness", desc.metallicRoughnessPath);
    texField("Normal", desc.normalPath);

    // Uniforms
    if (ImGui::CollapsingHeader("Uniforms", ImGuiTreeNodeFlags_DefaultOpen)) {
        int idx = 0;
        for (auto it = desc.vec4Uniforms.begin(); it != desc.vec4Uniforms.end();) {
            ImGui::PushID(idx++);
            char keyBuf[128]; strncpy(keyBuf, it->first.c_str(), sizeof(keyBuf)); keyBuf[sizeof(keyBuf)-1]=0;
            ImGui::InputText("Name", keyBuf, sizeof(keyBuf));
            DrawVec4Field("Value", it->second);
            bool remove = ImGui::Button("Remove");
            std::string newKey = keyBuf;
            ImGui::PopID();
            if (remove) { it = desc.vec4Uniforms.erase(it); continue; }
            if (newKey != it->first) {
                auto node = desc.vec4Uniforms.extract(it++);
                node.key() = newKey; desc.vec4Uniforms.insert(std::move(node));
            } else { ++it; }
        }
        if (ImGui::Button("+ Add Uniform")) {
            desc.vec4Uniforms["u_color"] = glm::vec4(1,1,1,1);
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Save")) {
        SaveMaterialAsset(path, desc);
    }
}


