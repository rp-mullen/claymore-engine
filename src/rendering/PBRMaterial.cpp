#include "PBRMaterial.h"

PBRMaterial::PBRMaterial(const std::string& name, bgfx::ProgramHandle program)
    : Material(name, program,
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
        BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA | BGFX_STATE_CULL_CCW)
{
    u_AlbedoSampler = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);
    u_MetallicRoughnessSampler = bgfx::createUniform("s_metallicRoughness", bgfx::UniformType::Sampler);
    u_NormalSampler = bgfx::createUniform("s_normalMap", bgfx::UniformType::Sampler);
   
    m_AlbedoTex = BGFX_INVALID_HANDLE;
    m_MetallicRoughnessTex = BGFX_INVALID_HANDLE;
    m_NormalTex = BGFX_INVALID_HANDLE;
   }

PBRMaterial::PBRMaterial(const std::string& name, bgfx::ProgramHandle program, uint64_t stateFlags)
    : Material(name, program, stateFlags) {
	u_AlbedoSampler = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);
	u_MetallicRoughnessSampler = bgfx::createUniform("s_metallicRoughness", bgfx::UniformType::Sampler);
	u_NormalSampler = bgfx::createUniform("s_normalMap", bgfx::UniformType::Sampler);

	m_AlbedoTex = BGFX_INVALID_HANDLE;
	m_MetallicRoughnessTex = BGFX_INVALID_HANDLE;
	m_NormalTex = BGFX_INVALID_HANDLE;
}

void PBRMaterial::SetAlbedoTexture(bgfx::TextureHandle texture) { m_AlbedoTex = texture; }
void PBRMaterial::SetMetallicRoughnessTexture(bgfx::TextureHandle texture) { m_MetallicRoughnessTex = texture; }
void PBRMaterial::SetNormalTexture(bgfx::TextureHandle texture) { m_NormalTex = texture; }

void PBRMaterial::BindUniforms() const
   {
   Material::BindUniforms();

   if (bgfx::isValid(m_AlbedoTex))
      bgfx::setTexture(0, u_AlbedoSampler, m_AlbedoTex);
   if (bgfx::isValid(m_MetallicRoughnessTex))
      bgfx::setTexture(1, u_MetallicRoughnessSampler, m_MetallicRoughnessTex);
   if (bgfx::isValid(m_NormalTex))
      bgfx::setTexture(2, u_NormalSampler, m_NormalTex);
   }
