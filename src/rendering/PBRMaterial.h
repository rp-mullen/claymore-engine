#pragma once
#include "Material.h"
#include <bgfx/bgfx.h>
#include <string>

class PBRMaterial : public Material
   {
   public:
      PBRMaterial(const std::string& name, bgfx::ProgramHandle program);

      PBRMaterial(const std::string& name,
          bgfx::ProgramHandle program,
          uint64_t stateFlags);

      void SetAlbedoTexture(bgfx::TextureHandle texture);
      void SetMetallicRoughnessTexture(bgfx::TextureHandle texture);
      void SetNormalTexture(bgfx::TextureHandle texture);

      // Convenience setters that also remember source paths for serialization
      void SetAlbedoTextureFromPath(const std::string& path);
      void SetMetallicRoughnessTextureFromPath(const std::string& path);
      void SetNormalTextureFromPath(const std::string& path);

      void BindUniforms() const override;
      
      bgfx::TextureHandle m_AlbedoTex;
      bgfx::TextureHandle m_MetallicRoughnessTex;
      bgfx::TextureHandle m_NormalTex;

      // Persistable source paths for textures (optional)
      const std::string& GetAlbedoPath() const { return m_AlbedoPath; }
      const std::string& GetMetallicRoughnessPath() const { return m_MetallicRoughnessPath; }
      const std::string& GetNormalPath() const { return m_NormalPath; }

   private:
      

      bgfx::UniformHandle u_AlbedoSampler;
      bgfx::UniformHandle u_MetallicRoughnessSampler;
      bgfx::UniformHandle u_NormalSampler;

      std::string m_AlbedoPath;
      std::string m_MetallicRoughnessPath;
      std::string m_NormalPath;
   };
 