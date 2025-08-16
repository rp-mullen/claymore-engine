#include "MaterialManager.h"
#include "ShaderManager.h"
#include "TextureLoader.h"

MaterialManager& MaterialManager::Instance() {
    static MaterialManager instance;
    return instance;
}

bgfx::VertexLayout MaterialManager::GetPBRVertexLayout() {
    static bgfx::VertexLayout layout;
    static bool initialized = false;

    if (!initialized) {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        initialized = true;
    }
    return layout;
}

std::shared_ptr<PBRMaterial> MaterialManager::CreateDefaultPBRMaterial() {
    static std::shared_ptr<PBRMaterial> defaultMaterial = nullptr;
    if (!defaultMaterial) {
        auto program = ShaderManager::Instance().LoadProgram("vs_pbr", "fs_pbr");
        defaultMaterial = std::make_shared<PBRMaterial>("DefaultPBR", program);

        bgfx::TextureHandle whiteTex{bgfx::kInvalidHandle};
        bgfx::TextureHandle mrTex{bgfx::kInvalidHandle};
        bgfx::TextureHandle normalTex{bgfx::kInvalidHandle};
        try { whiteTex = TextureLoader::Load2D("assets/debug/white.png"); } catch(...) { }
        try { mrTex = TextureLoader::Load2D("assets/debug/metallic_roughness.png"); } catch(...) { }
        try { normalTex = TextureLoader::Load2D("assets/debug/normal.png"); } catch(...) { }
        
        defaultMaterial->SetAlbedoTexture(whiteTex);
        defaultMaterial->SetMetallicRoughnessTexture(mrTex);
        defaultMaterial->SetNormalTexture(normalTex);
    }
    return defaultMaterial;
}

std::shared_ptr<SkinnedPBRMaterial> MaterialManager::CreateSkinnedPBRMaterial() {
    auto program = ShaderManager::Instance().LoadProgram("vs_pbr_skinned", "fs_pbr_skinned");
    auto mat = std::make_shared<SkinnedPBRMaterial>("SkinnedPBR", program);
    return mat;
}

std::shared_ptr<DebugMaterial> MaterialManager::CreateDefaultDebugMaterial() {
	static std::shared_ptr<DebugMaterial> defaultDebugMaterial = nullptr;
	if (!defaultDebugMaterial) {
		auto program = ShaderManager::Instance().LoadProgram("vs_debug", "fs_debug");
		defaultDebugMaterial = std::make_shared<DebugMaterial>("DefaultDebug", program);
	}
	return defaultDebugMaterial;
}