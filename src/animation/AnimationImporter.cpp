#include "animation/AnimationImporter.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/config.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "animation/AnimationAsset.h"
#include "animation/AnimationSerializer.h"

namespace cm {
namespace animation {

static glm::vec3 AiToGlm(const aiVector3D& v) { return glm::vec3(v.x, v.y, v.z); }
static glm::quat AiToGlm(const aiQuaternion& q) { return glm::quat(q.w, q.x, q.y, q.z); }

std::vector<AnimationClip> AnimationImporter::ImportFromModel(const std::string& filepath) {
    std::vector<AnimationClip> clips;

    Assimp::Importer importer;
    // FBX: ensure all animation stacks/takes are imported
    importer.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_OPTIMIZE_EMPTY_ANIMATION_CURVES, 1);
    importer.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, 0);
    const aiScene* scene = importer.ReadFile(filepath,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_LimitBoneWeights |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_FlipUVs);
    if (!scene || !scene->mRootNode) {
        std::cerr << "[AnimationImporter] Failed to open file: " << filepath << std::endl;
        return clips;
    }

    // Iterate over animations in the scene
    std::cout << "[AnimationImporter] Animations in '" << filepath << "': " << scene->mNumAnimations << "\n";
    for (unsigned int a = 0; a < scene->mNumAnimations; ++a) {
        const aiAnimation* aiAnim = scene->mAnimations[a];
        AnimationClip clip;
        clip.Name            = aiAnim->mName.C_Str();
        if (clip.Name.empty()) clip.Name = std::string("Anim_") + std::to_string(a);
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

        // Heuristic mark as humanoid if common tracks are observed (refined later by Avatar import stage)
        static const char* kSeeds[] = {"Hips","Spine","Neck","Head","LeftArm","RightArm","LeftUpLeg","RightUpLeg","LeftFoot","RightFoot"};
        for (const auto& [nm, tr] : clip.BoneTracks) {
            for (auto s : kSeeds) { if (nm.find(s) != std::string::npos) { clip.IsHumanoid = true; break; } }
            if (clip.IsHumanoid) break;
        }

        // If we later detect a matching humanoid avatar, project skeletal tracks into avatar tracks (HumanoidTracks)
        clips.push_back(std::move(clip));
    }

    return clips;
}

// New unified output: produce an .anim (unified) with BoneTracks from raw node channels
static AnimationAsset BuildUnifiedFromAssimp(const aiScene* scene, unsigned int animIndex)
{
    AnimationAsset asset; asset.meta.version = 1; asset.meta.fps = 30.0f; asset.meta.length = 0.0f;
    const aiAnimation* aiAnim = scene->mAnimations[animIndex];
    asset.name = aiAnim->mName.C_Str(); if (asset.name.empty()) asset.name = std::string("Anim_") + std::to_string(animIndex);
    const float tps = aiAnim->mTicksPerSecond != 0.0 ? (float)aiAnim->mTicksPerSecond : 25.0f;
    const float duration = (float)(aiAnim->mDuration / tps);
    asset.meta.length = duration;
    for (unsigned int c = 0; c < aiAnim->mNumChannels; ++c) {
        const aiNodeAnim* channel = aiAnim->mChannels[c];
        auto bt = std::make_unique<cm::animation::AssetBoneTrack>();
        bt->name = channel->mNodeName.C_Str();
        // Position
        for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k) {
            const aiVectorKey& key = channel->mPositionKeys[k];
            cm::animation::KeyVec3 kf; kf.id = 0; kf.t = (float)(key.mTime / tps); kf.v = glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z);
            bt->t.keys.push_back(kf);
        }
        // Rotation
        for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k) {
            const aiQuatKey& key = channel->mRotationKeys[k];
            cm::animation::KeyQuat kf; kf.id = 0; kf.t = (float)(key.mTime / tps); kf.v = glm::quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z);
            bt->r.keys.push_back(kf);
        }
        // Scale
        for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k) {
            const aiVectorKey& key = channel->mScalingKeys[k];
            cm::animation::KeyVec3 kf; kf.id = 0; kf.t = (float)(key.mTime / tps); kf.v = glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z);
            bt->s.keys.push_back(kf);
        }
        asset.tracks.push_back(std::move(bt));
    }
    return asset;
}

// Convenience: import a model and immediately save unified .anim next to it
bool AnimationImporter::ImportUnifiedAnimationFromFBX(const std::string& filepath, const std::string& outAnimPath)
{
    Assimp::Importer importer;
    importer.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_OPTIMIZE_EMPTY_ANIMATION_CURVES, 1);
    importer.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, 0);
    const aiScene* scene = importer.ReadFile(filepath,
        aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_LimitBoneWeights | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_FlipUVs);
    if (!scene || !scene->mRootNode || scene->mNumAnimations == 0) return false;

    cm::animation::AnimationAsset asset = BuildUnifiedFromAssimp(scene, 0);
    return cm::animation::SaveAnimationAsset(asset, outAnimPath);
}

} // namespace animation
} // namespace cm
