#include "editor/ui/AssetPicker.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>
#include <unordered_set>
#include "editor/Project.h"

namespace {
static std::vector<std::string> s_recent;

static bool MatchesGlob(const std::filesystem::path& p, const std::string& glob)
{
    // Very basic suffix match for patterns like "*.anim"; extend if needed
    if (glob.size() >= 2 && glob[0] == '*' && glob[1] == '.') {
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        std::string want = glob.substr(1); // e.g., ".anim"
        std::transform(want.begin(), want.end(), want.begin(), ::tolower);
        return ext == want;
    }
    // Fallback: accept all
    return true;
}
}

AssetPickerResult DrawAssetPicker(AssetPickerConfig cfg)
{
    AssetPickerResult result{};

    // Ensure unique ID scope per picker instance (prevents visible ID collisions when multiple pickers are drawn)
    ImGui::PushID(cfg.title ? cfg.title : (cfg.glob ? cfg.glob : "AssetPicker"));
    ImGui::TextUnformatted(cfg.title ? cfg.title : "Assets");
    ImGui::Separator();

    static std::string s_filter;
    char filterBuf[256];
    std::strncpy(filterBuf, s_filter.c_str(), sizeof(filterBuf));
    filterBuf[sizeof(filterBuf) - 1] = 0;
    ImGui::SetNextItemWidth(220);
    if (ImGui::InputText("Filter", filterBuf, sizeof(filterBuf))) {
        s_filter = filterBuf;
    }

    // List all matching assets from project roots
    std::vector<std::string> all;
    std::vector<std::filesystem::path> roots;
    // Prefer the configured asset directory; fall back to project root and default 'assets'
    auto assetDir = Project::GetAssetDirectory();
    if (!assetDir.empty()) roots.emplace_back(assetDir);
    const auto& projDir = Project::GetProjectDirectory();
    if (!projDir.empty()) roots.emplace_back(Project::GetProjectDirectory());
    roots.emplace_back("assets");

    std::error_code ec;
    for (const auto& root : roots) {
        if (!std::filesystem::exists(root, ec)) continue;
        for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
             it != std::filesystem::recursive_directory_iterator(); ++it) {
            if (!it->is_regular_file(ec)) continue;
            const auto& p = it->path();
            if (!MatchesGlob(p, cfg.glob ? cfg.glob : "*.*")) continue;
            std::string s = p.string();
            if (!s_filter.empty() && s.find(s_filter) == std::string::npos) continue;
            all.push_back(std::move(s));
        }
    }
    std::sort(all.begin(), all.end());

    // Recents
    if (cfg.showRecents && !s_recent.empty()) {
        ImGui::TextDisabled("Recent");
        ImGui::BeginChild("ap_recent", ImVec2(0, 64), true);
        for (const auto& r : s_recent) {
            if (!s_filter.empty() && r.find(s_filter) == std::string::npos) continue;
            if (ImGui::Selectable(r.c_str(), false)) {
                result.chosen = true; result.path = r; 
                // Move to front
                s_recent.erase(std::remove(s_recent.begin(), s_recent.end(), r), s_recent.end());
                s_recent.insert(s_recent.begin(), r);
                ImGui::EndChild();
                return result;
            }
        }
        ImGui::EndChild();
    }

    ImGui::TextDisabled("All");
    ImGui::BeginChild("ap_all", ImVec2(0, 180), true);
    for (const auto& a : all) {
        if (ImGui::Selectable(a.c_str(), false)) {
            result.chosen = true; result.path = a;
            // Update recents
            s_recent.erase(std::remove(s_recent.begin(), s_recent.end(), a), s_recent.end());
            s_recent.insert(s_recent.begin(), a);
            ImGui::EndChild();
            return result;
        }
    }

    // Accept drag-drop from Project panel or external
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE")) {
            const char* path = static_cast<const char*>(payload->Data);
            if (path && *path) {
                std::filesystem::path p(path);
                if (MatchesGlob(p, cfg.glob ? cfg.glob : "*.*")) {
                    result.chosen = true; result.path = p.string();
                    s_recent.erase(std::remove(s_recent.begin(), s_recent.end(), result.path), s_recent.end());
                    s_recent.insert(s_recent.begin(), result.path);
                    ImGui::EndDragDropTarget();
                    ImGui::EndChild();
                    return result;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();
    ImGui::PopID();

    return result;
}


