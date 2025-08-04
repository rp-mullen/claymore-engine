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
