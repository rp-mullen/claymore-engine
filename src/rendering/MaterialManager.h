#pragma once
#include <bgfx/bgfx.h>
#include <memory>
#include "PBRMaterial.h"
#include "DebugMaterial.h"
#include "SkinnedPBRMaterial.h"

class MaterialManager {
public:
    static MaterialManager& Instance();
    bgfx::VertexLayout GetPBRVertexLayout();
    
    std::shared_ptr<PBRMaterial> CreateDefaultPBRMaterial();
    std::shared_ptr<SkinnedPBRMaterial> CreateSkinnedPBRMaterial();
    // Explicit PSX creators for menus
    std::shared_ptr<class PBRMaterial> CreatePSXMaterial();
    std::shared_ptr<class SkinnedPBRMaterial> CreateSkinnedPSXMaterial();
    // Scene-preset aware creators (dispatch to PBR or PSX)
    std::shared_ptr<class Material> CreateSceneDefaultMaterial(class Scene* scene);
    std::shared_ptr<class Material> CreateSceneSkinnedDefaultMaterial(class Scene* scene);

    std::shared_ptr<DebugMaterial> CreateDefaultDebugMaterial();
};
