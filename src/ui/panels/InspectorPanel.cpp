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
#include "rendering/MaterialManager.h"
#include "rendering/MaterialAsset.h"
#include "rendering/TextureLoader.h"
#include <bgfx/bgfx.h>
#include "animation/AnimatorController.h"
#include "animation/AnimationPlayerComponent.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include "ui/panels/AvatarBuilderPanel.h"
// IK
#include "animation/ik/IKComponent.h"
// For project asset directory discovery and animation asset helpers
#include <editor/Project.h>
#include "animation/AnimationSerializer.h"
#include <filesystem>

// Convert raw field names (camelCase, PascalCase, snake_case) to spaced, capitalized labels
static std::string PrettifyLabel(const std::string& raw)
{
    if (raw.empty()) return raw;

    std::string spaced;
    spaced.reserve(raw.size() * 2);

    auto isUpper = [](char c){ return c >= 'A' && c <= 'Z'; };
    auto isLower = [](char c){ return c >= 'a' && c <= 'z'; };
    auto isDigit = [](char c){ return c >= '0' && c <= '9'; };

    for (size_t i = 0; i < raw.size(); ++i)
    {
        char c = raw[i];
        if (c == '_' || c == '-' || c == ' ')
        {
            if (!spaced.empty() && spaced.back() != ' ') spaced.push_back(' ');
            continue;
        }

        if (!spaced.empty())
        {
            char prev = spaced.back();
            char next = (i + 1 < raw.size()) ? raw[i + 1] : '\0';
            bool prevIsLetterOrDigit = ((prev >= 'A' && prev <= 'Z') || (prev >= 'a' && prev <= 'z') || (prev >= '0' && prev <= '9'));

            // Insert space on transitions like: aA, 0A, a0, A0, and between acronym boundary A a
            if (prevIsLetterOrDigit)
            {
                bool insert = false;
                if (isLower(prev) && isUpper(c)) insert = true;               // camelCase boundary
                else if (isDigit(prev) && !isDigit(c)) insert = true;         // digit -> letter
                else if (!isDigit(prev) && isDigit(c)) insert = true;         // letter -> digit
                else if (isUpper(prev) && isUpper(c) && isLower(next)) insert = true; // XMLh -> XML h
                if (insert && spaced.back() != ' ') spaced.push_back(' ');
            }
        }

        spaced.push_back(c);
    }

    // Title case
    std::string out; out.reserve(spaced.size());
    bool newWord = true;
    for (char c : spaced)
    {
        if (c == ' ')
        {
            if (!out.empty() && out.back() != ' ') out.push_back(' ');
            newWord = true;
        }
        else
        {
            if (newWord)
            {
                // Uppercase first letter
                if (c >= 'a' && c <= 'z') out.push_back(char(c - 'a' + 'A'));
                else out.push_back(c);
                newWord = false;
            }
            else
            {
                // Lowercase subsequent letters
                if (c >= 'A' && c <= 'Z') out.push_back(char(c - 'A' + 'a'));
                else out.push_back(c);
            }
        }
    }

    // Trim trailing space
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

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
    DrawInspectorContents();
    ImGui::End();
}

void InspectorPanel::OnImGuiRenderEmbedded() {
    DrawInspectorContents();
}

void InspectorPanel::DrawInspectorContents() {
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
        return;
    }

    // Only show scene preview when no entity is selected AND a scene file is selected.
    if ((!m_SelectedEntity || *m_SelectedEntity == -1) && !m_SelectedAssetPath.empty()) {
        std::string ext = std::filesystem::path(m_SelectedAssetPath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".scene") { DrawSceneFilePreview(); return; }
    }

    if (m_SelectedEntity && *m_SelectedEntity != -1 && m_Context) {
        // Entity header with rename-on-click
        if (auto* data = m_Context->GetEntityData(*m_SelectedEntity)) {
            ImGui::PushID((int)*m_SelectedEntity);
            if (!m_RenamingEntityName) {
                if (ImGui::Selectable(data->Name.c_str(), false)) {
                    m_RenamingEntityName = true;
                    strncpy(m_RenameBuffer, data->Name.c_str(), sizeof(m_RenameBuffer));
                    m_RenameBuffer[sizeof(m_RenameBuffer)-1] = 0;
                }
            } else {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
                ImGui::SetKeyboardFocusHere();
                if (ImGui::InputText("##rename_entity", m_RenameBuffer, IM_ARRAYSIZE(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                    std::string desired = m_RenameBuffer;
                    if (desired.empty()) desired = "Entity";
                    // ensure unique within scene
                    int suffix = 1; std::string finalName = desired; bool unique = false;
                    while (!unique) {
                        unique = true;
                        for (const auto& e : m_Context->GetEntities()) {
                            if (e.GetID() == *m_SelectedEntity) continue;
                            auto* ed = m_Context->GetEntityData(e.GetID());
                            if (ed && ed->Name == finalName) { unique = false; break; }
                        }
                        if (!unique) finalName = desired + "_" + std::to_string(suffix++);
                    }
                    data->Name = finalName;
                    m_RenamingEntityName = false;
                }
                if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0)) m_RenamingEntityName = false;
            }
            ImGui::PopID();
        }

        // Grouping (Layer/Tag/Groups)
        DrawGroupingControls(*m_SelectedEntity);

        DrawComponents(*m_SelectedEntity);
        // Offer to add an Animator if a skeleton exists but no AnimationPlayer is attached
        auto* data = m_Context->GetEntityData(*m_SelectedEntity);
        if (data && data->Skeleton && !data->AnimationPlayer) {
            ImGui::Separator();
            if (ImGui::Button("Add Animator to Entity")) {
                data->AnimationPlayer = std::make_unique<cm::animation::AnimationPlayerComponent>();
            }
        }
    }
    else {
        ImGui::Text("No entity selected.");
    }
}

void InspectorPanel::DrawSceneFilePreview() {
    using json = nlohmann::json;
    std::string ext = std::filesystem::path(m_SelectedAssetPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".scene") return;

    ImGui::Text("Scene File Preview");
    ImGui::Separator();
    ImGui::Text("%s", std::filesystem::path(m_SelectedAssetPath).filename().string().c_str());

    try {
        std::ifstream in(m_SelectedAssetPath);
        if (!in.is_open()) return;
        json j; in >> j; in.close();
        if (j.contains("entities") && j["entities"].is_array()) {
            const auto& ents = j["entities"];
            ImGui::Text("Entities: %d", (int)ents.size());
            // Show first N entity summaries
            int shown = 0;
            for (const auto& e : ents) {
                if (shown++ > 25) { ImGui::Text("... (%d more)", (int)ents.size() - 25); break; }
                std::string name = e.value("name", std::string("<unnamed>"));
                uint32_t id = e.value("id", 0u);
                ImGui::BulletText("[%u] %s", id, name.c_str());
                if (e.contains("mesh")) {
                    const auto& m = e["mesh"];
                    if (m.contains("meshReference")) {
                        const auto& r = m["meshReference"];
                        std::string guid = r.value("guid", std::string(""));
                        int fileID = r.value("fileID", 0);
                        ImGui::Text("  mesh guid: %s  fileID: %d", guid.c_str(), fileID);
                    }
                    if (m.contains("meshName")) {
                        ImGui::Text("  mesh name: %s", m.value("meshName", std::string()).c_str());
                    }
                }
            }
            // Scan for asset-looking strings
            std::vector<std::string> assets;
            std::function<void(const json&)> walk = [&](const json& n){
                if (n.is_string()) {
                    std::string s = n.get<std::string>();
                    std::string lower = s; for(char &c:lower) c = (char)tolower(c);
                    if (lower.find("assets/") != std::string::npos || lower.find(".fbx")!=std::string::npos || lower.find(".gltf")!=std::string::npos || lower.find(".png")!=std::string::npos)
                        assets.push_back(s);
                } else if (n.is_array()) for (const auto& e2 : n) walk(e2); else if (n.is_object()) for (auto it=n.begin(); it!=n.end(); ++it) walk(it.value());
            };
            walk(j);
            if (!assets.empty()) {
                ImGui::Separator();
                ImGui::Text("Referenced assets:");
                int shownA = 0; for (const auto& a : assets) { if (shownA++ > 30) { ImGui::Text("... (%d more)", (int)assets.size()-30); break; } ImGui::BulletText("%s", a.c_str()); }
            }
        }
    } catch(...) {}
}
void InspectorPanel::DrawGroupingControls(EntityID entity) {
    auto* data = m_Context->GetEntityData(entity);
    if (!data) return;

    if (ImGui::CollapsingHeader("Groups", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Layer", &data->Layer);
        // Tag
        char tagBuf[128];
        strncpy(tagBuf, data->Tag.c_str(), sizeof(tagBuf));
        tagBuf[sizeof(tagBuf)-1] = 0;
        if (ImGui::InputText("Tag", tagBuf, sizeof(tagBuf))) {
            data->Tag = tagBuf;
        }
        // Groups list with add/remove
        ImGui::Separator();
        ImGui::Text("Groups");
        int removeIndex = -1;
        for (int i = 0; i < (int)data->Groups.size(); ++i) {
            ImGui::PushID(i);
            char buf[128];
            strncpy(buf, data->Groups[i].c_str(), sizeof(buf));
            buf[sizeof(buf)-1] = 0;
            if (ImGui::InputText("##group", buf, sizeof(buf))) {
                data->Groups[i] = buf;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) removeIndex = i;
            ImGui::PopID();
        }
        if (removeIndex >= 0) data->Groups.erase(data->Groups.begin() + removeIndex);
        if (ImGui::SmallButton("Add Group")) data->Groups.push_back("");
    }
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
        MeshComponent* meshComp = data->Mesh.get();
        ImGui::TextDisabled("Material View");

        // Material select (built-in or user) for each slot (at least one)
        struct MatOpt { std::string name; std::string path; bool isBuiltIn; };
        static std::vector<MatOpt> s_options;
        s_options.clear();
        s_options.push_back({ "Default PBR", "<builtin:DefaultPBR>", true });
        s_options.push_back({ "Skinned PBR", "<builtin:SkinnedPBR>", true });
        s_options.push_back({ "PSX", "<builtin:PSX>", true });
        s_options.push_back({ "Skinned PSX", "<builtin:SkinnedPSX>", true });
        auto root = Project::GetAssetDirectory(); if (root.empty()) root = std::filesystem::path("assets");
        if (std::filesystem::exists(root)) {
            for (auto& p : std::filesystem::recursive_directory_iterator(root)) {
                if (!p.is_regular_file()) continue; auto ext = p.path().extension().string(); std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".mat") s_options.push_back({ p.path().stem().string(), p.path().string(), false });
            }
        }

        if (meshComp->materials.empty() && meshComp->material) meshComp->materials = { meshComp->material };

        // Slot selector dropdown to keep UI compact
        static int s_selectedSlot = 0;
        int maxSlots = (int)meshComp->materials.size(); if (maxSlots <= 0) maxSlots = 1;
        if (s_selectedSlot >= maxSlots) s_selectedSlot = maxSlots - 1;
        std::string slotLabel = std::string("Slot ") + std::to_string(s_selectedSlot);
        if (ImGui::BeginCombo("Material Slot", slotLabel.c_str())) {
            for (int i = 0; i < (int)meshComp->materials.size(); ++i) {
                bool sel = (i == s_selectedSlot);
                std::string entry = std::string("Slot ") + std::to_string(i);
                if (ImGui::Selectable(entry.c_str(), sel)) s_selectedSlot = i;
            }
            ImGui::EndCombo();
        }

        // Expose selected slot index to drawers via ImGui storage
        ImGui::GetStateStorage()->SetInt(ImGui::GetID("SelectedMaterialSlot"), s_selectedSlot);

        // Picker for the selected slot's material
        if ((size_t)s_selectedSlot < meshComp->materials.size()) {
            std::string curLabel = (meshComp->materials[s_selectedSlot] ? meshComp->materials[s_selectedSlot]->GetName() : std::string("<none>"));
            if (ImGui::BeginCombo("Material", curLabel.c_str())) {
                for (int i = 0; i < (int)s_options.size(); ++i) {
                    bool sel = false;
                    if (ImGui::Selectable(s_options[i].name.c_str(), sel)) {
                        std::shared_ptr<Material> newMat;
                        if (s_options[i].isBuiltIn) {
                            if (s_options[i].name == "Default PBR") newMat = MaterialManager::Instance().CreateDefaultPBRMaterial();
                            else if (s_options[i].name == "Skinned PBR") newMat = MaterialManager::Instance().CreateSkinnedPBRMaterial();
                            else if (s_options[i].name == "PSX") newMat = MaterialManager::Instance().CreatePSXMaterial();
                            else if (s_options[i].name == "Skinned PSX") newMat = MaterialManager::Instance().CreateSkinnedPSXMaterial();
                        } else {
                            MaterialAssetDesc desc; if (LoadMaterialAsset(s_options[i].path, desc)) newMat = CreateMaterialFromAsset(desc);
                        }
                        if (newMat) {
                            meshComp->materials[s_selectedSlot] = newMat;
                            if (s_selectedSlot == 0) meshComp->material = newMat; // keep primary in sync
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }

        // Draw compact per-slot UI via component drawer: shows textures etc. for the selected slot
        registry.DrawComponentUI("Mesh", meshComp);

        // PSX parameter UI only when PSX shaders are active on this material
        std::shared_ptr<Material> baseMat = (!meshComp->materials.empty() ? meshComp->materials[0] : meshComp->material);
        if (baseMat) {
            std::string n = baseMat->GetName();
            bool isPSX = (n == "PSX" || n == "SkinnedPSX");
            if (isPSX) {
                ImGui::Separator();
                ImGui::TextDisabled("PSX Parameters");
                // Read current values from material uniform if available
                glm::vec4 psx(0,0,0,0);
                baseMat->TryGetUniform("u_psxParams", psx);
                float jitter = psx.x;
                float affine = psx.y;
                if (ImGui::SliderFloat("Vertex Jitter (px)", &jitter, 0.0f, 4.0f, "%.1f")) {
                    psx.x = jitter; baseMat->SetUniform("u_psxParams", psx);
                }
                if (ImGui::SliderFloat("Affine Warp", &affine, 0.0f, 1.0f, "%.2f")) {
                    psx.y = affine; baseMat->SetUniform("u_psxParams", psx);
                }

                // Material Property Block overrides for PSX
                if (!meshComp->UniqueMaterial) {
                    ImGui::Separator();
                    ImGui::TextDisabled("PSX Overrides (Property Block)");
                    glm::vec4 pbPsx(0,0,0,0);
                    auto it = meshComp->PropertyBlock.Vec4Uniforms.find("u_psxParams");
                    if (it != meshComp->PropertyBlock.Vec4Uniforms.end()) pbPsx = it->second;
                    float jitterPB = pbPsx.x;
                    float affinePB = pbPsx.y;
                    bool changed = false;
                    changed |= ImGui::SliderFloat("Override Jitter (px)", &jitterPB, 0.0f, 4.0f, "%.1f");
                    changed |= ImGui::SliderFloat("Override Affine", &affinePB, 0.0f, 1.0f, "%.2f");
                    if (changed) meshComp->PropertyBlock.Vec4Uniforms["u_psxParams"] = glm::vec4(jitterPB, affinePB, 0.0f, 0.0f);
                }
            }
        }

        // Unique material toggle (applies to the selected slot's material instance)
        bool unique = meshComp->UniqueMaterial;
        if (ImGui::Checkbox("Unique Material", &unique)) {
            if (unique && !meshComp->UniqueMaterial) {
                if (!meshComp->materials.empty() && meshComp->materials[s_selectedSlot]) {
                    auto base = meshComp->materials[s_selectedSlot]; std::shared_ptr<Material> clone;
                    if (auto pbr = std::dynamic_pointer_cast<PBRMaterial>(base)) clone = std::make_shared<PBRMaterial>(*pbr);
                    else clone = base;
                    meshComp->materials[s_selectedSlot] = clone;
                    if (s_selectedSlot == 0) meshComp->material = clone;
                }
            }
            meshComp->UniqueMaterial = unique;
        }

        // Property overrides (MaterialPropertyBlock) on the mesh
        if (!meshComp->UniqueMaterial) {
            ImGui::Separator();
            ImGui::TextDisabled("Material Overrides (Property Block)");
            // Ensure slot arrays are sized
            if (meshComp->SlotPropertyBlocks.size() < meshComp->materials.size()) meshComp->SlotPropertyBlocks.resize(meshComp->materials.size());
            if (meshComp->SlotPropertyBlockTexturePaths.size() < meshComp->materials.size()) meshComp->SlotPropertyBlockTexturePaths.resize(meshComp->materials.size());

            MaterialPropertyBlock& pb = (s_selectedSlot < (int)meshComp->SlotPropertyBlocks.size()) ? meshComp->SlotPropertyBlocks[s_selectedSlot] : meshComp->PropertyBlock;
            auto& paths = (s_selectedSlot < (int)meshComp->SlotPropertyBlockTexturePaths.size()) ? meshComp->SlotPropertyBlockTexturePaths[s_selectedSlot] : meshComp->PropertyBlockTexturePaths;

            glm::vec4 tint(1.0f);
            auto itTint = pb.Vec4Uniforms.find("u_ColorTint");
            if (itTint != pb.Vec4Uniforms.end()) tint = itTint->second;
            if (ImGui::ColorEdit4("Tint", &tint.x)) pb.Vec4Uniforms["u_ColorTint"] = tint;

            bgfx::TextureHandle overrideTex = BGFX_INVALID_HANDLE;
            auto itTex = pb.Textures.find("s_albedo");
            if (itTex != pb.Textures.end()) overrideTex = itTex->second;
            ImGui::Text("Albedo Texture Override:");
            if (bgfx::isValid(overrideTex)) ImGui::ImageButton("OverrideTex", (ImTextureID)(uintptr_t)overrideTex.idx, ImVec2(64,64));
            else ImGui::Button("Drop texture", ImVec2(64,64));
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
                    const char* path = (const char*)payload->Data; if (path) {
                        bgfx::TextureHandle tex = TextureLoader::Load2D(path);
                        if (bgfx::isValid(tex)) { pb.Textures["s_albedo"] = tex; paths["s_albedo"] = std::string(path); }
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            if (data->Mesh) {
                data->Mesh->mesh = nullptr;
                data->Mesh->material.reset();
                data->Mesh.reset();
            }
        }
        ImGui::PopStyleColor(3);
    }

    if (data->UnifiedMorph && ImGui::CollapsingHeader("Unified Morphs")) {
        registry.DrawComponentUI("Unified Morphs", data->UnifiedMorph.get());
    }

    if (data->Light && ImGui::CollapsingHeader("Light")) {
        registry.DrawComponentUI("Light", data->Light.get());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            data->Light.reset();
        }
        ImGui::PopStyleColor(3);
    }

    if (data->Collider && ImGui::CollapsingHeader("Collider")) {
        registry.DrawComponentUI("Collider", data->Collider.get());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            data->Collider.reset();
        }
        ImGui::PopStyleColor(3);
    }

    if (data->Camera && ImGui::CollapsingHeader("Camera")) {
        registry.DrawComponentUI("Camera", data->Camera.get());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            data->Camera.reset();
        }
        ImGui::PopStyleColor(3);
    }

    if (data->RigidBody && ImGui::CollapsingHeader("RigidBody")) {
        registry.DrawComponentUI("RigidBody", data->RigidBody.get());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            data->RigidBody.reset();
        }
        ImGui::PopStyleColor(3);
    }

    if (data->StaticBody && ImGui::CollapsingHeader("StaticBody")) {
        registry.DrawComponentUI("StaticBody", data->StaticBody.get());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            data->StaticBody.reset();
        }
        ImGui::PopStyleColor(3);
    }

    if (data->Terrain && ImGui::CollapsingHeader("Terrain")) {
        registry.DrawComponentUI("Terrain", data->Terrain.get());
    }

    // UI Components
    if (data->Canvas && ImGui::CollapsingHeader("Canvas")) {
        registry.DrawComponentUI("Canvas", data->Canvas.get());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            data->Canvas.reset();
        }
        ImGui::PopStyleColor(3);
    }

    if (data->Panel && ImGui::CollapsingHeader("Panel")) {
        registry.DrawComponentUI("Panel", data->Panel.get());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            data->Panel.reset();
        }
        ImGui::PopStyleColor(3);
    }

    if (data->Button && ImGui::CollapsingHeader("Button")) {
        registry.DrawComponentUI("Button", data->Button.get());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            data->Button.reset();
        }
        ImGui::PopStyleColor(3);
    }

    // Particle System
    if (data->Emitter && ImGui::CollapsingHeader("Particle Emitter")) {
        registry.DrawComponentUI("ParticleEmitter", data->Emitter.get());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            if (ps::isValid(data->Emitter->Handle)) {
                ps::destroyEmitter(data->Emitter->Handle);
                data->Emitter->Handle = { uint16_t{UINT16_MAX} };
            }
            data->Emitter->Uniforms.reset();
            data->Emitter->Enabled = false;
            data->Emitter.reset();
        }
        ImGui::PopStyleColor(3);
    }

    // Text Renderer
    if (data->Text && ImGui::CollapsingHeader("TextRenderer")) {
        registry.DrawComponentUI("TextRenderer", data->Text.get());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            data->Text.reset();
        }
        ImGui::PopStyleColor(3);
    }

    // Navigation components
    if (data->Navigation && ImGui::CollapsingHeader("Nav Mesh")) {
        registry.DrawComponentUI("Nav Mesh", data->Navigation.get());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            data->Navigation.reset();
        }
        ImGui::PopStyleColor(3);
    }

    if (data->NavAgent && ImGui::CollapsingHeader("Nav Agent")) {
        registry.DrawComponentUI("Nav Agent", data->NavAgent.get());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0))) {
            data->NavAgent.reset();
        }
        ImGui::PopStyleColor(3);
    }

    // IK Components (multiple)
    if (!data->IKs.empty()) {
        if (ImGui::CollapsingHeader("IK")) {
            for (size_t i = 0; i < data->IKs.size(); ++i) {
                auto& ik = data->IKs[i];
                ImGui::PushID((int)i);
                if (ImGui::TreeNodeEx("IK Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Enabled", &ik.Enabled);
                    ImGui::DragFloat("Weight", &ik.Weight, 0.01f, 0.0f, 1.0f);
                    ImGui::Checkbox("Two Bone", &ik.UseTwoBone);
                    // Minimal display for target/pole
                    ImGui::Text("Target Entity: %u", (unsigned)ik.TargetEntity);
                    ImGui::Text("Pole Entity: %u", (unsigned)ik.PoleEntity);
                    if (ImGui::SmallButton("Remove IK")) {
                        data->IKs.erase(data->IKs.begin() + i);
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
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
        registry.DrawComponentUI("Animator", data->AnimationPlayer.get());

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
            data->Mesh = std::make_unique<MeshComponent>();
        }

        if (!data->Light && ImGui::MenuItem("Light Component")) {
            data->Light = std::make_unique<LightComponent>();
        }

        if (!data->Collider && ImGui::MenuItem("Collider Component")) {
            data->Collider = std::make_unique<ColliderComponent>();
        }

        if (!data->Camera && ImGui::MenuItem("Camera Component")) {
           data->Camera = std::make_unique<CameraComponent>();
           }

        if (!data->RigidBody && !data->StaticBody && ImGui::MenuItem("RigidBody Component")) {
            data->RigidBody = std::make_unique<RigidBodyComponent>();
            EnsureCollider(data->RigidBody.get(), data);

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
            data->StaticBody = std::make_unique<StaticBodyComponent>();
            EnsureCollider(data->StaticBody.get(), data);

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
            data->Emitter = std::make_unique<ParticleEmitterComponent>();
        }

        if (!data->Text && ImGui::MenuItem("TextRenderer Component")) {
            data->Text = std::make_unique<TextRendererComponent>();
        }

        // Navigation components
        if (!data->Navigation && ImGui::MenuItem("Nav Mesh Component")) {
            data->Navigation = std::make_unique<nav::NavMeshComponent>();
        }
        if (!data->NavAgent && ImGui::MenuItem("Nav Agent Component")) {
            data->NavAgent = std::make_unique<nav::NavAgentComponent>();
        }

        // IK authoring: add a new IK block on demand
        if (ImGui::MenuItem("IK Component")) {
            cm::animation::ik::IKComponent ik;
            data->IKs.push_back(std::move(ik));
        }

        // UI components
        if (!data->Canvas && ImGui::MenuItem("Canvas Component")) {
            data->Canvas = std::make_unique<CanvasComponent>();
        }
        if (!data->Panel && ImGui::MenuItem("Panel Component")) {
            data->Panel = std::make_unique<PanelComponent>();
        }
        if (!data->Button && ImGui::MenuItem("Button Component")) {
            data->Button = std::make_unique<ButtonComponent>();
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
    const std::string pretty = PrettifyLabel(property.name);
    
    switch (property.type) {
        case PropertyType::Int: {
            int value = std::get<int>(property.currentValue);
            if (ImGui::DragInt(pretty.c_str(), &value)) {
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
            if (ImGui::DragFloat(pretty.c_str(), &value, 0.1f)) {
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
            if (ImGui::Checkbox(pretty.c_str(), &value)) {
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
            
            if (ImGui::InputText(pretty.c_str(), buffer, sizeof(buffer))) {
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
            if (DrawVec3Control(pretty.c_str(), value)) {
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
            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 120.0f);
            ImGui::Text("%s", pretty.c_str());
            ImGui::NextColumn();
            ImGui::Button(btnLabel, ImVec2(-1,0));
            if(ImGui::BeginDragDropTarget()) {
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
                    EntityID dropped = *(EntityID*)payload->Data;
                    property.currentValue = static_cast<int>(dropped);
                    if(property.setter) property.setter(PropertyValue{ static_cast<int>(dropped) });
                    updated = true;
                }
                // Support dropping prefab asset to instantiate and assign
                if(const ImGuiPayload* payload2 = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
                    const char* path = (const char*)payload2->Data;
                    if (path) {
                        std::string p = path;
                        std::string ext = std::filesystem::path(p).extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".prefab") {
                            EntityID created = m_Context ? m_Context->InstantiateAsset(p, glm::vec3(0)) : -1;
                            if (created != -1) {
                                property.currentValue = static_cast<int>(created);
                                if(property.setter) property.setter(PropertyValue{ static_cast<int>(created) });
                                updated = true;
                            }
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::Columns(1);
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
