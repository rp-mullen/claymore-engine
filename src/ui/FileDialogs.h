#pragma once

#include <string>

// Simple OS-native file dialogs (Windows IFileDialog) with extension filters.
// Returns empty string on cancel or failure.

// Show an Open File dialog filtered to a single extension (e.g., extNoDot = L"anim").
std::string ShowOpenFileDialogExt(const wchar_t* description, const wchar_t* extNoDot);

// Show a Save File dialog filtered to a single extension (e.g., extNoDot = L"anim").
// defaultName should include the extension (e.g., L"NewAnimation.anim").
std::string ShowSaveFileDialogExt(const wchar_t* defaultName, const wchar_t* description, const wchar_t* extNoDot);


