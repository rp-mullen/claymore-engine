#pragma once
#include <bgfx/bgfx.h>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

class Material
{
public: 
    Material(const std::string& name,
             bgfx::ProgramHandle program, 
             uint64_t stateFlags = BGFX_STATE_DEFAULT)
        : m_Name(name), m_Program(program), m_StateFlags(stateFlags) {}

    void SetUniform(const std::string& name, const glm::vec4& value);
    virtual void BindUniforms() const;
    bool TryGetUniform(const std::string& name, glm::vec4& outValue) const;

    // Apply per-instance overrides before draw
    void ApplyPropertyBlock(const struct MaterialPropertyBlock& block) const;

    bgfx::ProgramHandle GetProgram() const { return m_Program; }
    uint64_t GetStateFlags() const { return m_StateFlags; }
	std::string GetName() const { return m_Name; }

    uint64_t m_StateFlags;

private:  
    std::string m_Name;
    bgfx::ProgramHandle m_Program;
    
    struct UniformData {
        bgfx::UniformHandle handle;
        glm::vec4 value;
    };
    std::unordered_map<std::string, UniformData> m_Uniforms;
};
