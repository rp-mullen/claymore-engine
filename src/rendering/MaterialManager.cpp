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

        bgfx::TextureHandle whiteTex = TextureLoader::Load2D("assets/debug/white.png");
        bgfx::TextureHandle mrTex = TextureLoader::Load2D("assets/debug/metallic_roughness.png");
        bgfx::TextureHandle normalTex = TextureLoader::Load2D("assets/debug/normal.png");
        
        defaultMaterial->SetAlbedoTexture(whiteTex);
        defaultMaterial->SetMetallicRoughnessTexture(mrTex);
        defaultMaterial->SetNormalTexture(normalTex);
    }
    return defaultMaterial;
}

std::shared_ptr<DebugMaterial> MaterialManager::CreateDefaultDebugMaterial() {
	static std::shared_ptr<DebugMaterial> defaultDebugMaterial = nullptr;
	if (!defaultDebugMaterial) {
		auto program = ShaderManager::Instance().LoadProgram("vs_debug", "fs_debug");
		defaultDebugMaterial = std::make_shared<DebugMaterial>("DefaultDebug", program);
	}
	return defaultDebugMaterial;
}