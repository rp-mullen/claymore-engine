#include "ShaderBundle.h"
#include "io/FileSystem.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

static bgfx::ShaderHandle CreateShaderFromFile(const fs::path& path) {
    std::vector<uint8_t> data;
    if (!FileSystem::Instance().ReadFile(path.string(), data)) return BGFX_INVALID_HANDLE;
    const bgfx::Memory* mem = bgfx::alloc(uint32_t(data.size()+1));
    if (!data.empty()) memcpy(mem->data, data.data(), data.size());
    mem->data[data.size()]='\0';
    return bgfx::createShader(mem);
}

ShaderBundle& ShaderBundle::Instance() {
    static ShaderBundle s; return s;
}

bgfx::ProgramHandle ShaderBundle::Load(const std::string& baseName) {
    auto it = m_Programs.find(baseName);
    if (it != m_Programs.end() && bgfx::isValid(it->second)) return it->second;

    fs::path exe = fs::current_path();
    fs::path vsBin = exe / "shaders" / "compiled" / "windows" / (baseName + ".vs.bin");
    fs::path fsBin = exe / "shaders" / "compiled" / "windows" / (baseName + ".fs.bin");
    bgfx::ShaderHandle vsh = CreateShaderFromFile(vsBin);
    bgfx::ShaderHandle fsh = CreateShaderFromFile(fsBin);
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        std::cerr << "[ShaderBundle] Missing compiled bins for " << baseName << std::endl;
        return BGFX_INVALID_HANDLE;
    }
    bgfx::ProgramHandle prog = bgfx::createProgram(vsh, fsh, true);
    m_Programs[baseName] = prog;
    return prog;
}

void ShaderBundle::Invalidate(const std::string& baseName) {
    auto it = m_Programs.find(baseName);
    if (it != m_Programs.end()) {
        if (bgfx::isValid(it->second)) bgfx::destroy(it->second);
        m_Programs.erase(it);
    }
}


