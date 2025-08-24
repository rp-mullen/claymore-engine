#include "CodeEditorPanel.h"
#include "../UILayer.h"
#include <fstream>
#include <imgui_internal.h>

void CodeEditorPanel::LoadFile() {
    m_Editor = std::make_unique<TextEditor>();
    // Language
    std::string ext = std::filesystem::path(m_FilePath).extension().string();
    for (auto& c : ext) c = (char)tolower(c);
    if (ext == ".cs") m_Editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    else if (ext == ".glsl" || ext == ".hlsl" || ext == ".shader") m_Editor->SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    else m_Editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());

    std::ifstream in(m_FilePath);
    if (in) {
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        m_Editor->SetText(content);
    }
    m_IsDirty = false;
}

bool CodeEditorPanel::SaveFile() {
    std::ofstream out(m_FilePath, std::ios::trunc);
    if (!out.is_open()) return false;
    out << m_Editor->GetText();
    out.close();
    m_IsDirty = false;
    return true;
}

void CodeEditorPanel::OnImGuiRender() {
    if (!m_Editor) return;

    std::string base = std::filesystem::path(m_FilePath).filename().string();
    if (m_IsDirty) base += "*";
    std::string title = base + std::string("###CodeEditor|") + m_FilePath;
    if (m_FocusNextFrame) { ImGui::SetNextWindowFocus(); m_FocusNextFrame = false; }
    if (!ImGui::Begin(title.c_str(), nullptr, ImGuiWindowFlags_MenuBar)) { ImGui::End(); return; }
    m_IsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem("Save", "Ctrl+S")) { SaveFile(); }
        ImGui::EndMenuBar();
    }

    // Dock into main dockspace on first show
    if (auto ui = m_UILayer) {
        static bool s_dockedOnce = false;
        if (!s_dockedOnce) {
            ImGui::DockBuilderDockWindow(title.c_str(), ui->GetMainDockspaceID());
            s_dockedOnce = true;
        }
    }

    // Change tracking
    if (m_Editor->IsTextChanged()) m_IsDirty = true;

    ImVec2 size = ImGui::GetContentRegionAvail();
    m_Editor->Render("TextEditor", size, true);
    ImGui::End();
}



