#pragma once
#include <string>
#include <memory>
#include <imgui.h>
#include <filesystem>
#include "EditorPanel.h"
#include "TextEditor.h"

class UILayer;

class CodeEditorPanel : public EditorPanel {
public:
    explicit CodeEditorPanel(const std::string& filePath, UILayer* ui)
        : m_FilePath(filePath), m_UILayer(ui) {
        LoadFile();
    }

    void OnImGuiRender();

    const std::string& GetFilePath() const { return m_FilePath; }
    void RequestFocus() { m_FocusNextFrame = true; }
    bool IsWindowFocusedOrHovered() const { return m_IsFocused; }

private:
    void LoadFile();
    bool SaveFile();
    const char* DetectLanguageFromExtension(const std::string& ext);

private:
    std::string m_FilePath;
    bool m_FocusNextFrame = false;
    bool m_IsFocused = false;
    bool m_IsDirty = false;
    std::unique_ptr<TextEditor> m_Editor;
    UILayer* m_UILayer = nullptr;
};



