#include "Project.h"
#include <fstream>
#include <iostream>
#include <core/application.h>

using json = nlohmann::json;

bool Project::Load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        std::cerr << "[Project] File does not exist: " << path << std::endl;
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Project] Failed to open project file: " << path << std::endl;
        return false;
    }

    json j;
    try {
        file >> j;
    }
    catch (const json::parse_error& e) {
        std::cerr << "[Project] JSON parse error: " << e.what() << std::endl;
        return false;
    }

    s_ProjectFile = path;
    s_ProjectDir = path.parent_path();
    s_ProjectName = j.value("name", "UnnamedProject");

    // assetDirectory is relative to .clayproj location
    std::string relAssetPath = j.value("assetDirectory", "assets");
    s_AssetDir = s_ProjectDir / relAssetPath;

    std::cout << "[Project] Loaded: " << s_ProjectName << std::endl;
    std::cout << "[Project] Root: " << s_ProjectDir << std::endl;
    std::cout << "[Project] Assets: " << s_AssetDir << std::endl;

	Application::Get().GetAssetWatcher()->SetRootPath(s_AssetDir.string());

    return true;
}

void Project::Save() {
    if (s_ProjectFile.empty()) {
        std::cerr << "[Project] No project file loaded. Cannot save." << std::endl;
        return;
    }

    json j;
    j["name"] = s_ProjectName;
    j["version"] = 1;
    j["assetDirectory"] = std::filesystem::relative(s_AssetDir, s_ProjectDir).string();

    std::ofstream out(s_ProjectFile);
    if (!out) {
        std::cerr << "[Project] Failed to save project to: " << s_ProjectFile << std::endl;
        return;
    }

    out << j.dump(4);
    out.close();
    std::cout << "[Project] Saved: " << s_ProjectFile << std::endl;
}

// Accessors
const std::filesystem::path& Project::GetProjectDirectory() {
    return s_ProjectDir;
}

const std::filesystem::path& Project::GetAssetDirectory() {
    return s_AssetDir;
}

const std::string& Project::GetProjectName() {
    return s_ProjectName;
}

const std::filesystem::path& Project::GetProjectFile() {
    return s_ProjectFile;
}
