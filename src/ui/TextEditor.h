#pragma once

#include <string>
#include <imgui.h>

// Lightweight text editor used by CodeEditorPanel.
// Provides only the subset of API that the panel uses.
class TextEditor {
public:
	struct LanguageDefinition {
		std::string name;
		static const LanguageDefinition& CPlusPlus();
		static const LanguageDefinition& GLSL();
	};

	TextEditor();

	void SetLanguageDefinition(const LanguageDefinition& definition);
	void SetText(const std::string& text);
	std::string GetText() const;

	// Matches external behavior: IsTextChanged() is read before Render() each frame.
	// Render() clears the internal changed flag at the beginning and sets it again
	// if the user modifies the text during this frame.
	bool IsTextChanged() const { return m_textChanged; }

	void Render(const char* title, const ImVec2& size = ImVec2(), bool border = false);

private:
	static int InputCallback(ImGuiInputTextCallbackData* data);

	std::string m_text;
	LanguageDefinition m_lang;
	mutable bool m_textChanged = false;
};


