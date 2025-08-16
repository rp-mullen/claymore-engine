#include "AssetRegistryPanel.h"
#include <string>

void AssetRegistryPanel::OnImGuiRender() {
    if (!ImGui::Begin("Asset Registry")) { ImGui::End(); return; }

    ImGui::Text("Registered assets:");
    ImGui::Separator();

    // Access AssetLibrary's internal map via a public method would be ideal; use PrintAllAssets() if needed.
    // For now, quickly render by asking the library to dump its list into UI: replicate PrintAllAssets with iteration.
    // We can't access private members here, so add minimal UI using exposed helpers.

    // Since there is no direct iterator, show a hint and allow user to print to console.
    if (ImGui::Button("Print to Console")) {
        AssetLibrary::Instance().PrintAllAssets();
    }

    ImGui::Separator();
    ImGui::Text("Inline view:");
    if (ImGui::BeginTable("assets", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, ImGui::GetContentRegionAvail().y - 10))) {
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("GUID");
        ImGui::TableSetupColumn("Type");
        ImGui::TableHeadersRow();
        auto list = AssetLibrary::Instance().GetAllAssets();
        for (const auto& row : list) {
            const std::string& path = std::get<0>(row);
            ClaymoreGUID guid = std::get<1>(row);
            AssetType type = std::get<2>(row);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(path.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(guid.ToString().c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%d", (int)type);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}


