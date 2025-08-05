#pragma once
#include <string>
#include "ParticleSystem.h"

namespace particles
{
    // Loads an image from disk (using stb_image) and creates an emitter sprite in the global
    // particle atlas. Returns an invalid handle on failure.
    ps::EmitterSpriteHandle LoadSprite(const std::string& path, bool flipY = true);
}
