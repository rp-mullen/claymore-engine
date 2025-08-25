#include "TextEditor.h"

#include <algorithm>

// Minimal language definitions just to satisfy current usage
const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::CPlusPlus() {
	static LanguageDefinition def{"C++"};
	return def;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::GLSL() {
	static LanguageDefinition def{"GLSL"};
	return def;
}

TextEditor::TextEditor() {
	m_lang = LanguageDefinition::CPlusPlus();
}

void TextEditor::SetLanguageDefinition(const LanguageDefinition& definition) {
	m_lang = definition;
}

void TextEditor::SetText(const std::string& text) {
	m_text = text;
	m_textChanged = true;
}

std::string TextEditor::GetText() const {
	return m_text;
}

int TextEditor::InputCallback(ImGuiInputTextCallbackData* data) {
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		// Resize string while keeping a stable data->Buf pointer
		std::string* str = reinterpret_cast<std::string*>(data->UserData);
		str->resize((size_t)data->BufTextLen);
		data->Buf = const_cast<char*>(str->c_str());
	}
	return 0;
}

void TextEditor::Render(const char* title, const ImVec2& size, bool border) {
	// Begin child region similar to external editor behavior
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	ImGui::BeginChild(title, size, border, ImGuiWindowFlags_HorizontalScrollbar);

	// Clear changed flag for this frame; will set to true if edits occur
	m_textChanged = false;

	// We need a mutable buffer for ImGui::InputTextMultiline. Use std::string with resize-callback.
	ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_NoUndoRedo;
	// Keep a working copy so we can detect changes
	std::string before = m_text;
	// Ensure the buffer has a null-terminator and capacity
	if (m_text.capacity() == 0) m_text.reserve(16);
	// Render textarea
	ImGui::InputTextMultiline("##code", m_text.data() ? const_cast<char*>(m_text.c_str()) : nullptr, m_text.size() + 1, ImGui::GetContentRegionAvail(), flags, InputCallback, &m_text);

	if (m_text != before) {
		m_textChanged = true;
	}

	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
}


