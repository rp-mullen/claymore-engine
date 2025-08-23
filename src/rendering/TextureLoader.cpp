#include "TextureLoader.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "io/FileSystem.h"
#include <stdexcept>
#include <algorithm>
#include <string>
#include <vector>
#include <cmath>
#include <cstdint>

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"


bgfx::TextureHandle TextureLoader::Load2D(const std::string& path, bool generateMips)
{
    int width, height, channels;
    // Try virtual filesystem first
    std::vector<uint8_t> fileData;
    stbi_uc* data = nullptr;
    if (FileSystem::Instance().ReadFile(path, fileData)) {
        data = stbi_load_from_memory(fileData.data(), static_cast<int>(fileData.size()), &width, &height, &channels, 4);
    } else {
        data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    }
    if (!data)
    {
        // Procedural fallbacks for engine default debug textures so export isn't hard-blocked by missing files
        auto ends_with = [](const std::string& s, const std::string& suffix) {
            if (suffix.size() > s.size()) return false;
            return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
        };
        std::vector<uint8_t> generated;
        if (ends_with(path, "assets/debug/white.png")) {
            width = height = 1; channels = 4; generated = { 255, 255, 255, 255 };
        } else if (ends_with(path, "assets/debug/metallic_roughness.png")) {
            // Default: non-metallic (0), roughness ~1.0 (255) in G channel; pack into RGBA as needed
            width = height = 1; channels = 4; generated = { 0, 255, 0, 255 };
        } else if (ends_with(path, "assets/debug/normal.png")) {
            // Default normal map value (0.5,0.5,1.0) -> (128,128,255)
            width = height = 1; channels = 4; generated = { 128, 128, 255, 255 };
        }

        if (!generated.empty()) {
            // Create texture from generated pixels
            bgfx::TextureHandle handle = bgfx::createTexture2D(
                static_cast<uint16_t>(width),
                static_cast<uint16_t>(height),
                generateMips,
                1,
                bgfx::TextureFormat::RGBA8,
                BGFX_TEXTURE_NONE,
                nullptr);
            const bgfx::Memory* mem = bgfx::copy(generated.data(), static_cast<uint32_t>(generated.size()));
            bgfx::updateTexture2D(handle, 0, 0, 0, 0,
                                  static_cast<uint16_t>(width),
                                  static_cast<uint16_t>(height),
                                  mem,
                                  static_cast<uint16_t>(width * 4));
            return handle;
        }

        throw std::runtime_error("Failed to load texture: " + path);
    }

    // Create an empty texture first – BGFX expects raw pixel uploads via updateTexture*.
    // Use wrap sampling so UI Panel Tile mode can repeat UVs beyond 1.0
    uint64_t kUITextureFlags = BGFX_TEXTURE_NONE;
    // Add this near the top of the file, after other bgfx includes if BGFX_SAMPLER_UVW_WRAP is not defined

    bgfx::TextureHandle handle = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        generateMips,
        1,
        bgfx::TextureFormat::RGBA8,
        kUITextureFlags,
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


bgfx::TextureHandle TextureLoader::LoadIconTexture(const std::string& path)
{
    // Detect file extension
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
        return s;
    };
    std::string lowerPath = toLower(path);
    bool isSvg = lowerPath.size() >= 4 && lowerPath.substr(lowerPath.size() - 4) == ".svg";


    if (isSvg)
    {
        // Parse SVG
        // Use 96 DPI and scale to target icon size
        constexpr float dpi = 96.0f;
        constexpr int targetSizePx = 64; // raster size; UI can scale down as needed

        // Load SVG via FileSystem if possible
        std::string svgText;
        NSVGimage* svg = nullptr;
        if (FileSystem::Instance().ReadTextFile(path, svgText)) {
            svg = nsvgParse(const_cast<char*>(svgText.c_str()), "px", dpi);
        } else {
            svg = nsvgParseFromFile(path.c_str(), "px", dpi);
        }
        if (svg == nullptr)
        {
            throw std::runtime_error("Failed to parse SVG icon: " + path);
        }

        const float w = svg->width;
        const float h = svg->height;
        const float maxDim = std::max(w, h);
        const float scale = maxDim > 0.0f ? static_cast<float>(targetSizePx) / maxDim : 1.0f;
        const int outW = std::max(1, static_cast<int>(std::ceil(w * scale)));
        const int outH = std::max(1, static_cast<int>(std::ceil(h * scale)));

        std::vector<uint8_t> rgba(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 4u, 0u);

        NSVGrasterizer* rast = nsvgCreateRasterizer();
        if (rast == nullptr)
        {
            nsvgDelete(svg);
            throw std::runtime_error("Failed to create NanoSVG rasterizer for: " + path);
        }

        nsvgRasterize(rast, svg, 0.0f, 0.0f, scale, reinterpret_cast<unsigned char*>(rgba.data()), outW, outH, outW * 4);

        nsvgDeleteRasterizer(rast);
        nsvgDelete(svg);

        // Create BGFX texture
        constexpr uint64_t kFlags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_UVW_CLAMP;
        bgfx::TextureHandle handle = bgfx::createTexture2D(
            static_cast<uint16_t>(outW),
            static_cast<uint16_t>(outH),
            false,
            1,
            bgfx::TextureFormat::RGBA8,
            kFlags,
            nullptr
        );

        const bgfx::Memory* mem = bgfx::copy(rgba.data(), static_cast<uint32_t>(rgba.size()));
        bgfx::updateTexture2D(handle, 0, 0, 0, 0,
                              static_cast<uint16_t>(outW),
                              static_cast<uint16_t>(outH),
                              mem,
                              static_cast<uint16_t>(outW * 4));

        return handle;
    }

    // Fallback: load raster image via stb_image
    int width, height, channels;
    std::vector<uint8_t> fileData;
    stbi_uc* data = nullptr;
    if (FileSystem::Instance().ReadFile(path, fileData)) {
        data = stbi_load_from_memory(fileData.data(), static_cast<int>(fileData.size()), &width, &height, &channels, 4);
    } else {
        data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    }
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