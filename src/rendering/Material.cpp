#include "Material.h"

void Material::SetUniform(const std::string& name, const glm::vec4& value)
{
    if (m_Uniforms.find(name) == m_Uniforms.end()) {
        m_Uniforms[name] = { bgfx::createUniform(name.c_str(), bgfx::UniformType::Vec4), value };
    } else {
        m_Uniforms[name].value = value;
    }
}

void Material::BindUniforms() const
{
    for (const auto& pair : m_Uniforms) {
        bgfx::setUniform(pair.second.handle, &pair.second.value);
    }
}

#include "MaterialPropertyBlock.h"
void Material::ApplyPropertyBlock(const MaterialPropertyBlock& block) const
{
    // Vec4 overrides
    for (const auto& kv : block.Vec4Uniforms)
    {
        bgfx::UniformHandle handle = BGFX_INVALID_HANDLE;
        auto it = m_Uniforms.find(kv.first);
        if (it != m_Uniforms.end())
        {
            handle = it->second.handle;
        }
        else
        {
            // Create transient uniform with this name
            handle = bgfx::createUniform(kv.first.c_str(), bgfx::UniformType::Vec4);
        }
        if (bgfx::isValid(handle))
        {
            bgfx::setUniform(handle, &kv.second);
        }
    }

    // Texture overrides: assumes sampler uniform with same name already exists in shader
    uint8_t slot = 0;
    for (const auto& kv : block.Textures)
    {
        bgfx::UniformHandle sampler = bgfx::createUniform(kv.first.c_str(), bgfx::UniformType::Sampler);
        if (bgfx::isValid(sampler) && bgfx::isValid(kv.second))
        {
            bgfx::setTexture(slot++, sampler, kv.second);
        }
    }
}
