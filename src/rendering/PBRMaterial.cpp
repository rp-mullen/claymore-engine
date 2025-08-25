#include "PBRMaterial.h"
#include "TextureLoader.h"

PBRMaterial::PBRMaterial(const std::string& name, bgfx::ProgramHandle program)
    : Material(name, program,
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
        BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA | BGFX_STATE_CULL_CW)
{
    u_AlbedoSampler = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);
    u_MetallicRoughnessSampler = bgfx::createUniform("s_metallicRoughness", bgfx::UniformType::Sampler);
    u_NormalSampler = bgfx::createUniform("s_normalMap", bgfx::UniformType::Sampler);
   
    m_AlbedoTex = BGFX_INVALID_HANDLE;
    m_MetallicRoughnessTex = BGFX_INVALID_HANDLE;
    m_NormalTex = BGFX_INVALID_HANDLE;
    // Default tint to white so shaders multiply by 1
    SetUniform("u_ColorTint", glm::vec4(1.0f));
   }

PBRMaterial::PBRMaterial(const std::string& name, bgfx::ProgramHandle program, uint64_t stateFlags)
    : Material(name, program, stateFlags) {
	u_AlbedoSampler = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);
	u_MetallicRoughnessSampler = bgfx::createUniform("s_metallicRoughness", bgfx::UniformType::Sampler);
	u_NormalSampler = bgfx::createUniform("s_normalMap", bgfx::UniformType::Sampler);

	m_AlbedoTex = BGFX_INVALID_HANDLE;
	m_MetallicRoughnessTex = BGFX_INVALID_HANDLE;
	m_NormalTex = BGFX_INVALID_HANDLE;
    // Default tint to white
    SetUniform("u_ColorTint", glm::vec4(1.0f));
}

void PBRMaterial::SetAlbedoTexture(bgfx::TextureHandle texture) { m_AlbedoTex = texture; }
void PBRMaterial::SetMetallicRoughnessTexture(bgfx::TextureHandle texture) { m_MetallicRoughnessTex = texture; }
void PBRMaterial::SetNormalTexture(bgfx::TextureHandle texture) { m_NormalTex = texture; }

void PBRMaterial::SetAlbedoTextureFromPath(const std::string& path) {
    m_AlbedoPath = path;
    bgfx::TextureHandle t = TextureLoader::Load2D(path);
    if (bgfx::isValid(t)) m_AlbedoTex = t;
}

void PBRMaterial::SetMetallicRoughnessTextureFromPath(const std::string& path) {
    m_MetallicRoughnessPath = path;
    bgfx::TextureHandle t = TextureLoader::Load2D(path);
    if (bgfx::isValid(t)) m_MetallicRoughnessTex = t;
}

void PBRMaterial::SetNormalTextureFromPath(const std::string& path) {
    m_NormalPath = path;
    bgfx::TextureHandle t = TextureLoader::Load2D(path);
    if (bgfx::isValid(t)) m_NormalTex = t;
}

void PBRMaterial::BindUniforms() const
   {
   Material::BindUniforms();

   // Ensure sane defaults so missing textures don't render black
   static bgfx::TextureHandle s_defaultWhite = BGFX_INVALID_HANDLE;
   static bgfx::TextureHandle s_defaultMR    = BGFX_INVALID_HANDLE;
   static bgfx::TextureHandle s_defaultNrm   = BGFX_INVALID_HANDLE;
   auto ensureDefaults = []()
   {
       if (!bgfx::isValid(s_defaultWhite))
           s_defaultWhite = TextureLoader::Load2D("assets/debug/white.png");
       if (!bgfx::isValid(s_defaultMR))
           s_defaultMR = TextureLoader::Load2D("assets/debug/metallic_roughness.png");
       if (!bgfx::isValid(s_defaultNrm))
           s_defaultNrm = TextureLoader::Load2D("assets/debug/normal.png");
   };
   ensureDefaults();

   bgfx::setTexture(0, u_AlbedoSampler, bgfx::isValid(m_AlbedoTex) ? m_AlbedoTex : s_defaultWhite);
   bgfx::setTexture(1, u_MetallicRoughnessSampler, bgfx::isValid(m_MetallicRoughnessTex) ? m_MetallicRoughnessTex : s_defaultMR);
   bgfx::setTexture(2, u_NormalSampler, bgfx::isValid(m_NormalTex) ? m_NormalTex : s_defaultNrm);
   }
