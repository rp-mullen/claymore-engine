#include "MaterialAsset.h"
#include "ShaderManager.h"
#include "TextureLoader.h"
#include "PBRMaterial.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

static void to_json(json& j, const MaterialAssetDesc& m) {
    j = json{
        {"name", m.name},
        {"shaderVS", m.shaderVS},
        {"shaderFS", m.shaderFS},
        {"albedo", m.albedoPath},
        {"metallicRoughness", m.metallicRoughnessPath},
        {"normal", m.normalPath}
    };
    // vec4 uniforms as name -> [x,y,z,w]
    json uniforms = json::object();
    for (const auto& kv : m.vec4Uniforms) {
        uniforms[kv.first] = { kv.second.x, kv.second.y, kv.second.z, kv.second.w };
    }
    j["uniforms"] = std::move(uniforms);
}

static void from_json(const json& j, MaterialAssetDesc& m) {
    if (j.contains("name")) m.name = j.at("name").get<std::string>();
    if (j.contains("shaderVS")) m.shaderVS = j.at("shaderVS").get<std::string>();
    if (j.contains("shaderFS")) m.shaderFS = j.at("shaderFS").get<std::string>();
    if (j.contains("albedo")) m.albedoPath = j.at("albedo").get<std::string>();
    if (j.contains("metallicRoughness")) m.metallicRoughnessPath = j.at("metallicRoughness").get<std::string>();
    if (j.contains("normal")) m.normalPath = j.at("normal").get<std::string>();
    if (j.contains("uniforms") && j["uniforms"].is_object()) {
        for (auto it = j["uniforms"].begin(); it != j["uniforms"].end(); ++it) {
            const auto& arr = it.value();
            if (arr.is_array() && arr.size() == 4) {
                m.vec4Uniforms[it.key()] = glm::vec4(arr[0], arr[1], arr[2], arr[3]);
            }
        }
    }
}

bool LoadMaterialAsset(const std::string& path, MaterialAssetDesc& out) {
    std::ifstream in(path);
    if (!in) return false;
    json j; in >> j;
    try { out = j.get<MaterialAssetDesc>(); }
    catch (...) { return false; }
    return true;
}

bool SaveMaterialAsset(const std::string& path, const MaterialAssetDesc& in) {
    std::ofstream outFile(path);
    if (!outFile) return false;
    json j = in;
    outFile << j.dump(4);
    return true;
}

std::shared_ptr<Material> CreateMaterialFromAsset(const MaterialAssetDesc& desc) {
    // Load shader program
    auto program = ShaderManager::Instance().LoadProgram(
        desc.shaderVS.empty() ? std::string("vs_pbr") : desc.shaderVS,
        desc.shaderFS.empty() ? std::string("fs_pbr") : desc.shaderFS);

    // Use PBRMaterial to support standard texture slots, while still allowing vec4 uniforms
    auto mat = std::make_shared<PBRMaterial>(desc.name.empty() ? std::string("Material") : desc.name, program);

    if (!desc.albedoPath.empty()) {
        bgfx::TextureHandle t = TextureLoader::Load2D(desc.albedoPath);
        if (bgfx::isValid(t)) static_cast<PBRMaterial*>(mat.get())->SetAlbedoTexture(t);
    }
    if (!desc.metallicRoughnessPath.empty()) {
        bgfx::TextureHandle t = TextureLoader::Load2D(desc.metallicRoughnessPath);
        if (bgfx::isValid(t)) static_cast<PBRMaterial*>(mat.get())->SetMetallicRoughnessTexture(t);
    }
    if (!desc.normalPath.empty()) {
        bgfx::TextureHandle t = TextureLoader::Load2D(desc.normalPath);
        if (bgfx::isValid(t)) static_cast<PBRMaterial*>(mat.get())->SetNormalTexture(t);
    }

    for (const auto& kv : desc.vec4Uniforms) {
        mat->SetUniform(kv.first, kv.second);
    }

    return mat;
}



