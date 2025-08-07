#include "animation/AnimationImporter.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace cm {
namespace animation {

static glm::vec3 AiToGlm(const aiVector3D& v) { return glm::vec3(v.x, v.y, v.z); }
static glm::quat AiToGlm(const aiQuaternion& q) { return glm::quat(q.w, q.x, q.y, q.z); }

std::vector<AnimationClip> AnimationImporter::ImportFromModel(const std::string& filepath) {
    std::vector<AnimationClip> clips;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath, aiProcess_Triangulate | aiProcess_FlipUVs);
    if (!scene || !scene->mRootNode) {
        std::cerr << "[AnimationImporter] Failed to open file: " << filepath << std::endl;
        return clips;
    }

    // Iterate over animations in the scene
    for (unsigned int a = 0; a < scene->mNumAnimations; ++a) {
        const aiAnimation* aiAnim = scene->mAnimations[a];
        AnimationClip clip;
        clip.Name            = aiAnim->mName.C_Str();
        clip.TicksPerSecond  = aiAnim->mTicksPerSecond != 0.0 ? (float)aiAnim->mTicksPerSecond : 25.0f;
        clip.Duration        = (float)(aiAnim->mDuration / clip.TicksPerSecond);

        // Iterate over each channel (bone)
        for (unsigned int c = 0; c < aiAnim->mNumChannels; ++c) {
            const aiNodeAnim* channel = aiAnim->mChannels[c];
            BoneTrack track;

            // Position keys
            for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k) {
                const aiVectorKey& key = channel->mPositionKeys[k];
                KeyframeVec3 kf;
                kf.Time  = (float)(key.mTime / clip.TicksPerSecond);
                kf.Value = AiToGlm(key.mValue);
                track.PositionKeys.push_back(kf);
            }

            // Rotation keys
            for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k) {
                const aiQuatKey& key = channel->mRotationKeys[k];
                KeyframeQuat kf;
                kf.Time  = (float)(key.mTime / clip.TicksPerSecond);
                kf.Value = AiToGlm(key.mValue);
                track.RotationKeys.push_back(kf);
            }

            // Scale keys
            for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k) {
                const aiVectorKey& key = channel->mScalingKeys[k];
                KeyframeVec3 kf;
                kf.Time  = (float)(key.mTime / clip.TicksPerSecond);
                kf.Value = AiToGlm(key.mValue);
                track.ScaleKeys.push_back(kf);
            }

            clip.BoneTracks[channel->mNodeName.C_Str()] = std::move(track);
        }

        clips.push_back(std::move(clip));
    }

    return clips;
}

} // namespace animation
} // namespace cm
