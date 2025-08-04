#include "ConsolePanel.h"
#include <imgui.h>

void ConsolePanel::OnImGuiRender() {
    ImGui::Begin("Console");

    // Toolbar
    if (ImGui::Button("Clear")) Clear();
    ImGui::SameLine();
    ImGui::Checkbox("Info", &m_ShowInfo); ImGui::SameLine();
    ImGui::Checkbox("Warning", &m_ShowWarning); ImGui::SameLine();
    ImGui::Checkbox("Error", &m_ShowError);
    ImGui::SameLine();
    ImGui::InputTextWithHint("##Search", "Search...", m_SearchBuffer, sizeof(m_SearchBuffer));

    ImGui::Separator();

    ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& entry : m_LogEntries) {
        // Filtering
        if ((entry.level == LogLevel::Info && !m_ShowInfo) ||
            (entry.level == LogLevel::Warning && !m_ShowWarning) ||
            (entry.level == LogLevel::Error && !m_ShowError))
            continue;
        if (strlen(m_SearchBuffer) > 0 && entry.message.find(m_SearchBuffer) == std::string::npos)
            continue;

        // Color based on log level
        ImVec4 color;
        switch (entry.level) {
        case LogLevel::Info: color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); break;
        case LogLevel::Warning: color = ImVec4(1.0f, 0.8f, 0.3f, 1.0f); break;
        case LogLevel::Error: color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        if (entry.count > 1)
            ImGui::Text("[%d] %s", entry.count, entry.message.c_str());
        else
            ImGui::Text("%s", entry.message.c_str());
        ImGui::PopStyleColor();
    }

    if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}

void ConsolePanel::AddLog(const std::string& message, LogLevel level) {
    if (m_LogIndex.find(message) != m_LogIndex.end()) {
        int idx = m_LogIndex[message];
        m_LogEntries[idx].count++;
    }
    else {
        m_LogIndex[message] = (int)m_LogEntries.size();
        m_LogEntries.push_back({ message, level, 1 });
    }
}

void ConsolePanel::Clear() {
    m_LogEntries.clear();
    m_LogIndex.clear();
}
