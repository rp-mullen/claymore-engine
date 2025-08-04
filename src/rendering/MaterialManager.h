#pragma once
#include <bgfx/bgfx.h>
#include <memory>
#include "PBRMaterial.h"
#include "DebugMaterial.h"

class MaterialManager {
public:
    static MaterialManager& Instance();
    bgfx::VertexLayout GetPBRVertexLayout();
    
    std::shared_ptr<PBRMaterial> CreateDefaultPBRMaterial();

    std::shared_ptr<DebugMaterial> CreateDefaultDebugMaterial();
};
