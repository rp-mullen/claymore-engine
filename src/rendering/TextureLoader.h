#pragma once
// Ensure Windows macro LoadIcon does not collide with our method name
#ifdef LoadIcon
#undef LoadIcon
#endif

#include <bgfx/bgfx.h>
#include <string>
#include <imgui.h>

class TextureLoader
   {
   public:
      // When loading raw 2-D images we create only the base level by default.
      // Pass generateMips=true if you upload your own full mip-chain.
      static bgfx::TextureHandle Load2D(const std::string& path, bool generateMips = false);
      static bgfx::TextureHandle LoadIconTexture(const std::string& path);
      static ImTextureID ToImGuiTextureID(bgfx::TextureHandle handle);
   };
