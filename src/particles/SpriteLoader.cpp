#include "SpriteLoader.h"
#include <stb_image.h>
#include <iostream>

namespace particles
{
    ps::EmitterSpriteHandle LoadSprite(const std::string& path, bool flipY)
    {
        stbi_set_flip_vertically_on_load(flipY);
        int width, height, channels;
        stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (!data)
        {
            std::cerr << "[SpriteLoader] Failed to load image: " << path << std::endl;
            return { UINT16_MAX };
        }

        ps::EmitterSpriteHandle sprite = ps::createSprite((uint16_t)width, (uint16_t)height, data);
        stbi_image_free(data);
        return sprite;
    }
}
