#include "SkinnedPBRMaterial.h"
#include "TextureLoader.h"

SkinnedPBRMaterial::SkinnedPBRMaterial(const std::string& name, bgfx::ProgramHandle program)
    : PBRMaterial(name, program,
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
        BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA | BGFX_STATE_CULL_CCW)
{
    u_Bones = bgfx::createUniform("u_bones", bgfx::UniformType::Mat4, MaxBones);
    m_BoneCount = 0;

    // Provide sensible default textures so unassigned samplers don\'t sample garbage.
    bgfx::TextureHandle whiteTex   = TextureLoader::Load2D("assets/debug/white.png");
    bgfx::TextureHandle mrTex      = TextureLoader::Load2D("assets/debug/metallic_roughness.png");
    bgfx::TextureHandle normalTex  = TextureLoader::Load2D("assets/debug/normal.png");
    SetAlbedoTexture(whiteTex);
    SetMetallicRoughnessTexture(mrTex);
    SetNormalTexture(normalTex);
}  

void SkinnedPBRMaterial::UploadBones(const std::vector<glm::mat4>& boneMatrices)
{
    m_BoneCount = std::min<uint32_t>(boneMatrices.size(), MaxBones);
    for (uint32_t i = 0; i < m_BoneCount; ++i)
        m_Palette[i] = boneMatrices[i];
}

void SkinnedPBRMaterial::BindUniforms() const
{
    PBRMaterial::BindUniforms();
    if (m_BoneCount)
        bgfx::setUniform(u_Bones, m_Palette.data(), m_BoneCount);
}
