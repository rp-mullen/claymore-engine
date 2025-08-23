#pragma once
#include <bgfx/bgfx.h>
#include <glm/glm.hpp>
#include <vector>
#include <limits>
#include <iostream>

struct Mesh {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE; // may store static or dynamic handle casted
    bgfx::DynamicVertexBufferHandle dvbh = BGFX_INVALID_HANDLE; // valid when Dynamic=true
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    uint32_t numVertices = 0;
    uint32_t numIndices = 0;
    bool Dynamic = false;

    // CPU-side data for bounds & picking / morph targets / skinning
    std::vector<glm::vec3> Vertices;
    std::vector<glm::vec3> Normals;
    std::vector<glm::vec2> UVs;
    std::vector<uint32_t> Indices;

    // Skinning (optional)
    std::vector<glm::vec4> BoneWeights; // xyzw weight
    std::vector<glm::ivec4> BoneIndices;

    glm::vec3 BoundsMin;
    glm::vec3 BoundsMax;

    bool HasSkinning() const { return !BoneWeights.empty(); }

    void ComputeBounds() {
        if (Vertices.empty()) {
            BoundsMin = glm::vec3(0);
            BoundsMax = glm::vec3(0);
            return;
        }
        glm::vec3 min(std::numeric_limits<float>::max());
        glm::vec3 max(std::numeric_limits<float>::lowest());
        for (auto& v : Vertices) {
            min = glm::min(min, v);
            max = glm::max(max, v);
        }
        BoundsMin = min;
        BoundsMax = max;
    }

    // GPU resource lifetime for meshes is managed by owning systems (asset pipeline or managers).
    // Avoid destroying handles here to prevent use-after-free during in-flight frames.
};