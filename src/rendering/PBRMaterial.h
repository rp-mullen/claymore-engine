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

      void BindUniforms() const override;
      
      bgfx::TextureHandle m_AlbedoTex;
      bgfx::TextureHandle m_MetallicRoughnessTex;
      bgfx::TextureHandle m_NormalTex;

   private:
      

      bgfx::UniformHandle u_AlbedoSampler;
      bgfx::UniformHandle u_MetallicRoughnessSampler;
      bgfx::UniformHandle u_NormalSampler;
   };
 