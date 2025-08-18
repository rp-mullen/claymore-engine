#pragma once

#include <bgfx/bgfx.h>
#include <string>
#include <unordered_map>

// Lightweight bundle loader reading shaders/meta/<Name>.json and compiled bins
// Creates bgfx shaders/program and caches by base name.

struct ShaderBundleMeta {
    bool skinned = false;
    std::unordered_map<std::string, std::string> renderState;
};

class ShaderBundle {
public:
    static ShaderBundle& Instance();

    bgfx::ProgramHandle Load(const std::string& baseName);
    void Invalidate(const std::string& baseName);

private:
    ShaderBundle() = default;
    std::unordered_map<std::string, bgfx::ProgramHandle> m_Programs;
};


