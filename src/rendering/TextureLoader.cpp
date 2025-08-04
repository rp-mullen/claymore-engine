#include "TextureLoader.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stdexcept>

bgfx::TextureHandle TextureLoader::Load2D(const std::string& path, bool generateMips)
{
    int width, height, channels;
    stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data)
    {
        throw std::runtime_error("Failed to load texture: " + path);
    }

    // Create an empty texture first – BGFX expects raw pixel uploads via updateTexture*.
    bgfx::TextureHandle handle = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        generateMips,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE /* flags */,
        nullptr            /* no initial data */
    );

    // Upload the pixel data to the base mip level (layer 0).
    const bgfx::Memory* mem = bgfx::copy(data, width * height * 4);
    bgfx::updateTexture2D(
        handle,
        0,                /* layer */
        0,                /* mip  */
        0, 0,             /* x, y */
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        mem,
        static_cast<uint16_t>(width * 4) /* pitch */
    );

    stbi_image_free(data);
    return handle;
}


bgfx::TextureHandle TextureLoader::LoadIcon(const std::string& path)
{
    int width, height, channels;
    stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data)
    {
        throw std::runtime_error("Failed to load icon texture: " + path);
    }

    // Icons don't need mipmaps; create empty texture with clamp sampler flags.
    constexpr uint64_t kFlags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_UVW_CLAMP;

    bgfx::TextureHandle handle = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false, // no mips
        1,
        bgfx::TextureFormat::RGBA8,
        kFlags,
        nullptr /* no initial data */
    );

    const bgfx::Memory* mem = bgfx::copy(data, width * height * 4);
    bgfx::updateTexture2D(
        handle,
        0, 0, 0, 0,
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        mem,
        static_cast<uint16_t>(width * 4)
    );

    stbi_image_free(data);
    return handle;
}


// Utility to convert TextureHandle to ImGui texture ID
ImTextureID TextureLoader::ToImGuiTextureID(bgfx::TextureHandle handle)
   {
   return (ImTextureID)(uintptr_t)handle.idx;
   }