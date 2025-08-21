// IKSolvers.h
#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "animation/ik/IKTypes.h"

struct SkeletonComponent;

namespace cm { namespace animation { namespace ik {

struct TwoBoneInputs {
    glm::vec3 rootPos;
    glm::vec3 midPos;
    glm::vec3 endPos;
    glm::vec3 targetPos;
    glm::vec3 polePos;
    bool hasPole = false;
    float upperLen = 0.0f;
    float lowerLen = 0.0f;
};

// Returns desired local rotations for root and mid (end orientation derived from aim)
bool SolveTwoBone(const TwoBoneInputs& in,
                  const JointConstraint* jointConstraintsOptional,
                  glm::quat& outRootLocal, glm::quat& outMidLocal,
                  float& outError);

// FABRIK for N>=2. Provide initial joint world positions, target, and optional pole.
// On success, outputs new joint world positions (including effector) and final error.
bool SolveFABRIK(std::vector<glm::vec3>& jointWorld,
                 const glm::vec3& target,
                 int maxIterations,
                 float tolerance,
                 const glm::vec3* poleOptional,
                 float& outError,
                 int& outIterations);

// Utility to derive local rotations from parent-child world transforms
void WorldChainToLocalRots(const std::vector<glm::mat4>& parentWorld,
                           const std::vector<glm::vec3>& jointWorld,
                           std::vector<glm::quat>& outLocalRots);

} } }


