#pragma once

#include <string>
#include <vector>
#include "animation/AnimationTypes.h"

namespace cm {
namespace animation {

class AnimationImporter {
public:
    // Extract all animations from a model file (fbx, gltf, etc.) using Assimp
    static std::vector<AnimationClip> ImportFromModel(const std::string& filepath);
};

} // namespace animation
} // namespace cm
