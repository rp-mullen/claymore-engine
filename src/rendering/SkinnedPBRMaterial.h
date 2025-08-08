#pragma once
#include "PBRMaterial.h"
#include <array>

class SkinnedPBRMaterial : public PBRMaterial
{
public:
    static constexpr uint32_t MaxBones = 128;

    SkinnedPBRMaterial(const std::string& name, bgfx::ProgramHandle program);

    // Provide palette each frame before rendering. Size must be <= MaxBones
    void UploadBones(const std::vector<glm::mat4>& boneMatrices);

    void BindUniforms() const override;

private:
    bgfx::UniformHandle u_Bones;
    std::array<glm::mat4, MaxBones> m_Palette{};
    uint32_t m_BoneCount = 0;
};
