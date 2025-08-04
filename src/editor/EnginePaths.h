// EnginePaths.h
#pragma once
#include <filesystem>
#include <string>

class EnginePaths {
public:
    static void Init(const std::string& executablePath);

    static const std::filesystem::path& GetEngineRoot();   // bin/Debug
    static std::filesystem::path GetEngineAssetPath();      // bin/Debug/assets/

private:
    static std::filesystem::path s_EngineRoot;
};
