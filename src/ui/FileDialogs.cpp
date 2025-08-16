#include "ui/FileDialogs.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shobjidl.h>
#include <string>

static std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s; s.resize((size_t)len);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), len, nullptr, nullptr);
    return s;
}

std::string ShowOpenFileDialogExt(const wchar_t* description, const wchar_t* extNoDot)
{
    IFileDialog* pFileDialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileDialog));
    if (FAILED(hr) || !pFileDialog) return {};

    // Filter
    COMDLG_FILTERSPEC filterSpec[1];
    std::wstring pattern = L"*."; pattern += extNoDot ? extNoDot : L"*";
    filterSpec[0].pszName = description ? description : L"Files";
    filterSpec[0].pszSpec = pattern.c_str();
    pFileDialog->SetFileTypes(1, filterSpec);

    hr = pFileDialog->Show(nullptr);
    if (FAILED(hr)) { pFileDialog->Release(); return {}; }

    IShellItem* pItem = nullptr;
    hr = pFileDialog->GetResult(&pItem);
    if (FAILED(hr) || !pItem) { pFileDialog->Release(); return {}; }

    PWSTR pszFilePath = nullptr;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    std::string path;
    if (SUCCEEDED(hr) && pszFilePath) path = WideToUtf8(pszFilePath);
    if (pszFilePath) CoTaskMemFree(pszFilePath);
    pItem->Release();
    pFileDialog->Release();
    return path;
}

std::string ShowSaveFileDialogExt(const wchar_t* defaultName, const wchar_t* description, const wchar_t* extNoDot)
{
    IFileDialog* pFileDialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileDialog));
    if (FAILED(hr) || !pFileDialog) return {};

    // Filter
    COMDLG_FILTERSPEC filterSpec[1];
    std::wstring pattern = L"*."; pattern += extNoDot ? extNoDot : L"*";
    filterSpec[0].pszName = description ? description : L"Files";
    filterSpec[0].pszSpec = pattern.c_str();
    pFileDialog->SetFileTypes(1, filterSpec);
    if (extNoDot) pFileDialog->SetDefaultExtension(extNoDot);

    if (defaultName && defaultName[0]) pFileDialog->SetFileName(defaultName);

    hr = pFileDialog->Show(nullptr);
    if (FAILED(hr)) { pFileDialog->Release(); return {}; }

    IShellItem* pItem = nullptr;
    hr = pFileDialog->GetResult(&pItem);
    if (FAILED(hr) || !pItem) { pFileDialog->Release(); return {}; }

    PWSTR pszFilePath = nullptr;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    std::string path;
    if (SUCCEEDED(hr) && pszFilePath) path = WideToUtf8(pszFilePath);
    if (pszFilePath) CoTaskMemFree(pszFilePath);
    pItem->Release();
    pFileDialog->Release();
    return path;
}


