#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>

// Simple editable material asset description persisted as .mat (JSON)
// Focused on PBR-like usage with optional custom uniforms.
struct MaterialAssetDesc {
    std::string name;
    std::string shaderVS;   // e.g. "vs_pbr" or "vs_pbr_skinned"
    std::string shaderFS;   // e.g. "fs_pbr"

    // Common PBR texture slots
    std::string albedoPath;
    std::string metallicRoughnessPath;
    std::string normalPath;

    // Optional parameter block (vec4 uniforms) keyed by uniform name
    std::unordered_map<std::string, glm::vec4> vec4Uniforms;
};

// IO helpers
bool LoadMaterialAsset(const std::string& path, MaterialAssetDesc& out);
bool SaveMaterialAsset(const std::string& path, const MaterialAssetDesc& in);

// Runtime creation helper
class Material;
std::shared_ptr<Material> CreateMaterialFromAsset(const MaterialAssetDesc& desc);


