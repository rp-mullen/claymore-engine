#pragma once

#include "EditorPanel.h"
#include "utils/Profiler.h"
#include <imgui.h>

class ProfilerPanel : public EditorPanel {
public:
	void OnImGuiRender() {
		if (!m_Open) return;
		Profiler& prof = Profiler::Get();
		ImGui::Begin("Profiler", &m_Open);
		bool enabled = prof.IsEnabled();
		if (ImGui::Checkbox("Enabled", &enabled)) prof.SetEnabled(enabled);
		ImGui::SameLine();
		if (ImGui::Button("Refresh")) {
			// No-op; panel reads latest samples each frame
		}

		Profiler::MemoryStats mem = prof.GetProcessMemory();
		ImGui::Text("Working Set: %.2f MB", mem.workingSetBytes / (1024.0 * 1024.0));
		ImGui::SameLine();
		ImGui::Text("Private: %.2f MB", mem.privateBytes / (1024.0 * 1024.0));
		ImGui::Separator();

		if (ImGui::BeginTable("cpu", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
			ImGui::TableSetupColumn("Scope");
			ImGui::TableSetupColumn("Total (ms)");
			ImGui::TableSetupColumn("Calls");
			ImGui::TableHeadersRow();

			auto rows = prof.GetSortedLastFrameEntriesByTimeDesc();
			for (const auto& e : rows) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(e.name.c_str());
				ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", e.totalMs);
				ImGui::TableSetColumnIndex(2); ImGui::Text("%u", e.callCount);
			}
			ImGui::EndTable();
		}

		ImGui::End();
	}

	void Open() { m_Open = true; }
	bool IsOpen() const { return m_Open; }

private:
	bool m_Open = false;
};


