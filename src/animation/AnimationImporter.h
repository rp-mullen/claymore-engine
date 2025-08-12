#pragma once

#include <string>
#include <vector>
#include "animation/AnimationTypes.h"
#include "animation/AnimationAsset.h"

namespace cm {
namespace animation {

class AnimationImporter {
public:
    // Extract all animations from a model file (fbx, gltf, etc.) using Assimp
    static std::vector<AnimationClip> ImportFromModel(const std::string& filepath);
    // New unified import convenience: build a single unified AnimationAsset and save it
    static bool ImportUnifiedAnimationFromFBX(const std::string& filepath, const std::string& outAnimPath);
};

} // namespace animation
} // namespace cm
