#pragma once

#include <unordered_map>
#include <string>
#include <glm/glm.hpp>
#include <bgfx/bgfx.h>

// Lightweight per-renderer override collection similar to Unity's MaterialPropertyBlock.
struct MaterialPropertyBlock {
    // Name -> vec4 uniform overrides
    std::unordered_map<std::string, glm::vec4> Vec4Uniforms;
    // Name -> texture overrides (sampler assumed vec4 in shader)
    std::unordered_map<std::string, bgfx::TextureHandle> Textures;

    bool Empty() const { return Vec4Uniforms.empty() && Textures.empty(); }

    void Clear() {
        Vec4Uniforms.clear();
        Textures.clear();
    }
};
