#include <memory>
#include <glm.hpp>

struct Environment {
   enum class AmbientMode {
      FlatColor,
      Skybox
      };

   AmbientMode Ambient = AmbientMode::FlatColor;
   glm::vec3 AmbientColor = glm::vec3(0.2f);
   float AmbientIntensity = 1.0f;

   bool UseSkybox = true;
   std::shared_ptr<TextureCube> SkyboxTexture = nullptr;

   float Exposure = 1.0f;
   bool EnableFog = false;
   // Add more: fog color, scattering, tonemapping, etc.
   };
