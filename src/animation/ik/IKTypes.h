// IKTypes.h
#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using EntityID = uint32_t;

namespace cm { namespace animation { struct PoseBuffer; } }
struct SkeletonComponent;

namespace cm { namespace animation { namespace ik {

using BoneId = int32_t;

constexpr int kMaxChainLen = 32;

struct JointConstraint {
    float twistMinDeg = 0.0f;
    float twistMaxDeg = 0.0f;
    float hingeMinDeg = 0.0f;
    float hingeMaxDeg = 0.0f;
    bool useTwist = false;
    bool useHinge = false;
};

struct ChainConfig {
    std::vector<BoneId> bones; // ordered root..effector
    std::vector<JointConstraint> constraints; // size = bones.size()-1 (per joint)
    bool useTwoBone = true;
    int maxIterations = 12;
    float tolerance = 0.001f; // meters
};

struct ChainRuntimeCache {
    glm::vec3 lastEffectorPos{0.0f};
    glm::quat lastSolvedRots[kMaxChainLen];
    bool wasValidLastFrame = false;
    int cachedLen = 0;
};

struct ChainInputs {
    glm::vec3 targetWorld{0.0f};
    glm::vec3 poleWorld{0.0f};
    bool hasPole = false;
    float weight = 1.0f; // blend 0..1
    float damping = 0.2f; // 0..1
};

struct ChainSolved {
    // Desired local-space rotations for each bone in chain
    std::vector<glm::quat> localRots; // size = bones.size()
    float error = 0.0f;
    int iterations = 0;
    bool valid = false;
};

// Utility decompose/compose
struct TRS { glm::vec3 T{0.0f}; glm::quat R{1,0,0,0}; glm::vec3 S{1.0f}; };

} } } // namespaces


