#pragma once
#include "Material.h"

class DebugMaterial : public Material {
public:
    DebugMaterial(const std::string& name, bgfx::ProgramHandle program)
        : Material(name, program,
            BGFX_STATE_WRITE_RGB | BGFX_STATE_PT_LINES | BGFX_STATE_DEPTH_TEST_LEQUAL) {
    }

    void BindUniforms() const override {
        // No custom uniforms for now
    }
};
