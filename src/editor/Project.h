#pragma once
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

class Project {
public:


    static bool Load(const std::filesystem::path& path);
    static void Save(); // Optional, if you want to update the .clayproj

    static const std::filesystem::path& GetProjectDirectory();
    static const std::filesystem::path& GetAssetDirectory();
    static const std::string& GetProjectName();
    static const std::filesystem::path& GetProjectFile();

	static void SetProjectDirectory(const std::filesystem::path& path) {
		s_ProjectDir = path;
		s_AssetDir = path / "assets"; // Default asset directory
	}

private:
    static inline std::string s_ProjectName;
    static inline std::filesystem::path s_ProjectFile;
    static inline std::filesystem::path s_ProjectDir;
    static inline std::filesystem::path s_AssetDir;
};
