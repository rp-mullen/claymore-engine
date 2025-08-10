#include "AvatarBuilderPanel.h"
#include <imgui.h>
#include "ecs/Scene.h"
#include "ecs/EntityData.h"
#include "ecs/Components.h"
#include "animation/AvatarDefinition.h"
#include "animation/AvatarSerializer.h"

using namespace cm::animation;

AvatarBuilderPanel::AvatarBuilderPanel(Scene* scene)
{
    SetContext(scene);
}

void AvatarBuilderPanel::SetContext(Scene* scene)
{
    m_Scene = scene;
}

void AvatarBuilderPanel::OpenForEntity(int entityId)
{
    m_TargetEntity = entityId;
    m_Open = true;
    m_Working = std::make_unique<AvatarDefinition>();
    if (m_Scene) {
        if (auto* data = m_Scene->GetEntityData(entityId)) {
            if (data->Skeleton) {
                avatar_builders::BuildFromSkeleton(*data->Skeleton, *m_Working, true);
            }
        }
    }
}

void AvatarBuilderPanel::OnImGuiRender()
{
    if (!m_Open) return;
    if (!m_Scene) { ImGui::Begin("Avatar Builder"); ImGui::Text("No scene."); ImGui::End(); return; }
    ImGui::Begin("Avatar Builder", &m_Open);
    if (m_TargetEntity == -1) {
        ImGui::TextDisabled("No target entity selected. Use Inspector to open Avatar Builder.");
        ImGui::End();
        return;
    }

    auto* data = m_Scene->GetEntityData(m_TargetEntity);
    if (!data || !data->Skeleton) {
        ImGui::TextDisabled("Selected entity has no skeleton.");
        ImGui::End();
        return;
    }

    SkeletonComponent& skel = *data->Skeleton;

    // Toolbar
    if (ImGui::Button("Auto-map")) AutoMap(skel);
    ImGui::SameLine(); if (ImGui::Button("Validate")) Validate(skel);
    ImGui::SameLine(); if (ImGui::Button("Save Avatar")) SaveAvatar(skel);
    ImGui::Separator();

    DrawMappingUI(skel);

    ImGui::End();
}

void AvatarBuilderPanel::DrawMappingUI(SkeletonComponent& skel)
{
    if (!m_Working) return;
    ImGui::Text("Rig: %s", m_Working->RigName.c_str());
    ImGui::TextDisabled("Units per meter: %.3f", m_Working->UnitsPerMeter);
    ImGui::Separator();

    // Simple table: HumanoidBone index -> name
    if (ImGui::BeginTable("map", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Bone");
        ImGui::TableSetupColumn("Mapped Name");
        ImGui::TableSetupColumn("Index");
        ImGui::TableHeadersRow();
        for (uint16_t i = 0; i < HumanoidBoneCount; ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(ToString(static_cast<HumanoidBone>(i)));
            ImGui::TableSetColumnIndex(1);
            std::string& name = m_Working->Map[i].BoneName;
            char buf[128]; std::snprintf(buf, sizeof(buf), "%s", name.c_str());
            if (ImGui::InputText((std::string("##n")+std::to_string(i)).c_str(), buf, sizeof(buf))) {
                name = buf;
                int idx = skel.GetBoneIndex(name);
                m_Working->Map[i].BoneIndex = idx;
                m_Working->Present[i] = (idx >= 0);
            }
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", m_Working->Map[i].BoneIndex);
        }
        ImGui::EndTable();
    }
}

void AvatarBuilderPanel::AutoMap(SkeletonComponent& skel)
{
    if (!m_Working) m_Working = std::make_unique<AvatarDefinition>();
    avatar_builders::BuildFromSkeleton(skel, *m_Working, true);
}

void AvatarBuilderPanel::Validate(SkeletonComponent& skel)
{
    int missingRequired = 0;
    for (uint16_t i = 0; i < HumanoidBoneCount; ++i) {
        HumanoidBone b = static_cast<HumanoidBone>(i);
        if (IsHumanoidBoneRequired(b) && !m_Working->Present[i]) ++missingRequired;
    }
    if (missingRequired > 0) ImGui::OpenPopup("AvatarValidation");
    if (ImGui::BeginPopupModal("AvatarValidation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Missing required bones: %d", missingRequired);
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void AvatarBuilderPanel::SaveAvatar(SkeletonComponent& skel)
{
    // Persist working avatar to file next to source (simple heuristic)
    std::string out = std::string("assets/") + (m_Working->RigName.empty() ? std::string("Avatar") : m_Working->RigName) + ".avatar";
    ::cm::animation::SaveAvatar(*m_Working, out);
}


