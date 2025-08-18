#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

// Simple JSON-based material asset referencing a unified .shader
// Stores default parameter values and texture bindings by logical slot or sampler name.

struct MaterialAssetUnified {
    std::string name;
    std::string shaderPath; // project-relative path to .shader
    std::unordered_map<std::string, glm::vec4> params; // name -> default vec4 (floats coerced)
    std::unordered_map<std::string, std::string> textures; // tag/name -> vpath to texture asset
};

namespace MaterialImporter {
    bool Load(const std::string& path, MaterialAssetUnified& out);
    bool Save(const std::string& path, const MaterialAssetUnified& in);
}


