// EnginePaths.cpp
#include "EnginePaths.h"

std::filesystem::path EnginePaths::s_EngineRoot;

void EnginePaths::Init(const std::string& executablePath) {
    s_EngineRoot = std::filesystem::absolute(std::filesystem::path(executablePath)).parent_path();
}

const std::filesystem::path& EnginePaths::GetEngineRoot() {
    return s_EngineRoot;
}

std::filesystem::path EnginePaths::GetEngineAssetPath() {
    return s_EngineRoot / "assets";
}
