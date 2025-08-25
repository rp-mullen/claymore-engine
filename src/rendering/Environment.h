#pragma once
#include <memory>
#include <glm/glm.hpp>

class TextureCube; // forward declaration

struct Environment {
    enum class AmbientMode {
        FlatColor,
        Skybox
    };

    AmbientMode Ambient = AmbientMode::FlatColor;
    glm::vec3 AmbientColor = glm::vec3(0.2f);
    float AmbientIntensity = 1.0f;

    bool UseSkybox = false;
    std::shared_ptr<TextureCube> SkyboxTexture = nullptr;

    // Exposure/tonemapping placeholder
    float Exposure = 1.0f;

    // Fog
    bool EnableFog = false;
    glm::vec3 FogColor = glm::vec3(0.5f, 0.6f, 0.7f);
    float FogDensity = 0.02f; // exponential fog density

    // Procedural skybox
    bool ProceduralSky = false;
    glm::vec3 SkyZenithColor = glm::vec3(0.2f, 0.35f, 0.6f);
    glm::vec3 SkyHorizonColor = glm::vec3(0.7f, 0.85f, 1.0f);

    // Screen-space outline (cosmetic)
    bool OutlineEnabled = false;
    glm::vec3 OutlineColor = glm::vec3(0.0f, 0.0f, 0.0f);
    float OutlineThickness = 2.0f; // pixels (1..8)
};
