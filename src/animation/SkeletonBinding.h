#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <span>
#include "pipeline/AssetReference.h"
#include "ecs/AnimationComponents.h"
#include "rendering/Mesh.h"

// Simple 64-bit FNV-1a hash for stable joint GUIDs from strings
uint64_t Hash64(const std::string& s);
uint64_t Hash64_CombineGuidPath(const ClaymoreGUID& guid, const std::string& fullPath);

// Lookups (stubs; integrate with asset system as needed)
const SkeletonComponent* GetSkeleton(const ClaymoreGUID& guid);
const Mesh* GetMesh(const ClaymoreGUID& guid);
// Animation clip placeholder type
struct AnimClip { std::string Name; float Duration = 0.0f; float SampleRate = 0.0f; };
const AnimClip* GetClip(const ClaymoreGUID& guid);

// Binding helpers
bool BuildBoneRemap(const Mesh& mesh,
                    const SkeletonComponent& skel,
                    std::vector<uint16_t>& outRemap,
                    std::vector<uint16_t>& outUsedJointList);

void BuildBonePaletteBuffer(const SkeletonComponent& skel,
                            const std::vector<uint16_t>& usedJointList,
                            std::vector<glm::mat4>& outPalette);

// Animator contract (light wrappers)
namespace cm { namespace animation { class Animator; } }
void Animator_AttachSkeleton(cm::animation::Animator& a, const SkeletonComponent& skel);
void Animator_Tick(cm::animation::Animator& a, float dt);

// Compute globals from locals (depending on engine's transform conventions)
void Skeleton_ComputeGlobals(const SkeletonComponent& skel,
                             const std::vector<glm::mat4>& localTransforms,
                             std::vector<glm::mat4>& outGlobals);

// Utility to compute per-joint full path strings and jointGuids
void ComputeSkeletonJointGuids(SkeletonComponent& skel);


