#include "MaterialManager.h"
#include "ShaderManager.h"
#include "TextureLoader.h"
#include "ecs/Scene.h"

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

// Scene-preset aware creators (temporarily only PBR; PSX added later)
std::shared_ptr<Material> MaterialManager::CreateSceneDefaultMaterial(Scene* scene) {
    if (scene && scene->GetDefaultShaderPreset() == Scene::ShaderPreset::PSX) {
        auto program = ShaderManager::Instance().LoadProgram("vs_psx", "fs_psx");
        auto mat = std::make_shared<PBRMaterial>("PSX", program);
        // Defaults similar to PBR for texture slots
        try { static_cast<PBRMaterial*>(mat.get())->SetAlbedoTexture(TextureLoader::Load2D("assets/debug/white.png")); } catch(...) {}
        return mat;
    }
    return CreateDefaultPBRMaterial();
}

std::shared_ptr<Material> MaterialManager::CreateSceneSkinnedDefaultMaterial(Scene* scene) {
    if (scene && scene->GetDefaultShaderPreset() == Scene::ShaderPreset::PSX) {
        auto program = ShaderManager::Instance().LoadProgram("vs_psx_skinned", "fs_psx");
        auto mat = std::make_shared<SkinnedPBRMaterial>("SkinnedPSX", program);
        try { static_cast<SkinnedPBRMaterial*>(mat.get())->SetAlbedoTexture(TextureLoader::Load2D("assets/debug/white.png")); } catch(...) {}
        return mat;
    }
    return CreateSkinnedPBRMaterial();
}

std::shared_ptr<PBRMaterial> MaterialManager::CreatePSXMaterial() {
    auto program = ShaderManager::Instance().LoadProgram("vs_psx", "fs_psx");
    auto mat = std::make_shared<PBRMaterial>("PSX", program);
    try { mat->SetAlbedoTexture(TextureLoader::Load2D("assets/debug/white.png")); } catch(...) {}
    return mat;
}

std::shared_ptr<SkinnedPBRMaterial> MaterialManager::CreateSkinnedPSXMaterial() {
    auto program = ShaderManager::Instance().LoadProgram("vs_psx_skinned", "fs_psx");
    auto mat = std::make_shared<SkinnedPBRMaterial>("SkinnedPSX", program);
    try { mat->SetAlbedoTexture(TextureLoader::Load2D("assets/debug/white.png")); } catch(...) {}
    return mat;
}