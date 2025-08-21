// IKSolvers.cpp
// References:
//  - Two-bone IK using law of cosines: see e.g., Müller, Eberly; also Unity/Unreal implementations
//  - FABRIK: Forward And Backward Reaching Inverse Kinematics (Aristidou & Lasenby, 2011)

#include "animation/ik/IKSolvers.h"

#define GLM_ENABLE_EXPERIMENTAL 
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>

namespace cm { namespace animation { namespace ik {

static inline glm::quat FromTo(const glm::vec3& a, const glm::vec3& b) {
    glm::vec3 na = glm::length2(a) > 1e-12f ? glm::normalize(a) : glm::vec3(1,0,0);
    glm::vec3 nb = glm::length2(b) > 1e-12f ? glm::normalize(b) : glm::vec3(1,0,0);
    float d = glm::clamp(glm::dot(na, nb), -1.0f, 1.0f);
    if (d > 1.0f - 1e-6f) return glm::quat(1,0,0,0);
    if (d < -1.0f + 1e-6f) {
        // 180-deg: choose arbitrary orthogonal axis
        glm::vec3 axis = glm::abs(na.x) < 0.99f ? glm::normalize(glm::cross(na, glm::vec3(1,0,0)))
                                                : glm::normalize(glm::cross(na, glm::vec3(0,1,0)));
        return glm::angleAxis(glm::pi<float>(), axis);
    }
    glm::vec3 axis = glm::normalize(glm::cross(na, nb));
    float angle = std::acos(d);
    return glm::angleAxis(angle, axis);
}

bool SolveTwoBone(const TwoBoneInputs& in,
                  const JointConstraint* /*jointConstraintsOptional*/,
                  glm::quat& outRootLocal, glm::quat& outMidLocal,
                  float& outError)
{
    // Build triangle sides
    const glm::vec3 root = in.rootPos;
    const glm::vec3 mid  = in.midPos;
    const glm::vec3 end  = in.endPos;
    const glm::vec3 target = in.targetPos;

    float upper = in.upperLen > 0.0f ? in.upperLen : glm::length(mid - root);
    float lower = in.lowerLen > 0.0f ? in.lowerLen : glm::length(end - mid);
    float maxReach = upper + lower;
    glm::vec3 rootToTarget = target - root;
    float dist = glm::length(rootToTarget);
    float tdist = glm::clamp(dist, 1e-6f, maxReach);

    // Law of cosines for the elbow angle
    float cosAngle = glm::clamp((upper*upper + lower*lower - tdist*tdist)/(2.0f*upper*lower), -1.0f, 1.0f);
    float elbow = std::acos(cosAngle);

    // Desired plane: defined by root->target and pole (if provided)
    glm::vec3 fwd = (tdist > 1e-6f) ? (rootToTarget / tdist) : glm::vec3(1,0,0);
    glm::vec3 up = glm::vec3(0,1,0);
    if (in.hasPole) {
        glm::vec3 rootToPole = in.polePos - root;
        glm::vec3 proj = rootToPole - fwd * glm::dot(rootToPole, fwd);
        if (glm::length2(proj) > 1e-10f) up = glm::normalize(proj);
    }
    glm::vec3 right = glm::normalize(glm::cross(up, fwd));
    up = glm::normalize(glm::cross(fwd, right));

    // Construct desired joint positions in plane
    float a = upper; float b = lower; float c = tdist;
    float cosAtRoot = glm::clamp((b*b + c*c - a*a)/(2.0f*b*c), -1.0f, 1.0f);
    float rootAngle = std::acos(cosAtRoot);
    glm::vec3 midDesired = root + fwd * (std::cos(rootAngle) * a) + up * (std::sin(rootAngle) * a);
    glm::vec3 endDesired = target; // effector at target (clamped along line if out of reach handled via c==maxReach)

    // Compute local rotations to align current directions to desired
    glm::vec3 curDirUpper = mid - root;
    glm::vec3 curDirLower = end - mid;
    glm::vec3 newDirUpper = midDesired - root;
    glm::vec3 newDirLower = endDesired - midDesired;
    glm::quat rotRoot = FromTo(curDirUpper, newDirUpper);
    glm::quat rotMid  = FromTo(curDirLower, newDirLower);

    outRootLocal = glm::normalize(rotRoot);
    outMidLocal  = glm::normalize(rotMid);
    outError = glm::length(endDesired - end);
    return true;
}

bool SolveFABRIK(std::vector<glm::vec3>& jointWorld,
                 const glm::vec3& target,
                 int maxIterations,
                 float tolerance,
                 const glm::vec3* poleOptional,
                 float& outError,
                 int& outIterations)
{
    const int n = (int)jointWorld.size();
    if (n < 2) { outError = 0; outIterations = 0; return false; }

    // Segment lengths
    std::vector<float> segLen(n-1);
    float total = 0.0f;
    for (int i=0;i<n-1;++i) { segLen[i] = glm::length(jointWorld[i+1]-jointWorld[i]); total += segLen[i]; }
    glm::vec3 root = jointWorld[0];

    // If target unreachable, align and stretch
    float dist = glm::length(target - root);
    if (dist >= total - 1e-6f) {
        glm::vec3 dir = (dist>1e-6f)? (target-root)/dist : glm::vec3(1,0,0);
        for (int i=1;i<n;++i) jointWorld[i] = jointWorld[i-1] + dir * segLen[i-1];
        outError = glm::length(jointWorld.back() - target);
        outIterations = 0;
        return true;
    }

    std::vector<glm::vec3> b = jointWorld;
    outIterations = 0;
    for (int iter=0; iter<maxIterations; ++iter) {
        // Backward reaching: set effector to target
        jointWorld.back() = target;
        for (int i=n-2;i>=0;--i) {
            glm::vec3 dir = glm::normalize(jointWorld[i] - jointWorld[i+1]);
            jointWorld[i] = jointWorld[i+1] + dir * segLen[i];
        }
        // Forward reaching: lock root, move forward
        jointWorld[0] = root;
        for (int i=0;i<n-1;++i) {
            glm::vec3 dir = glm::normalize(jointWorld[i+1] - jointWorld[i]);
            jointWorld[i+1] = jointWorld[i] + dir * segLen[i];
        }
        // Optional pole stabilization: project middle joints toward pole plane
        if (poleOptional) {
            glm::vec3 pole = *poleOptional;
            for (int i=1;i<n-1;++i) {
                glm::vec3 a = jointWorld[i-1];
                glm::vec3 b2 = jointWorld[i+1];
                glm::vec3 ab = glm::normalize(b2 - a);
                glm::vec3 ap = pole - a;
                glm::vec3 proj = a + ab * glm::dot(ap, ab);
                glm::vec3 dir = jointWorld[i] - proj;
                glm::vec3 dirPole = pole - proj;
                if (glm::length2(dir)>1e-10f && glm::length2(dirPole)>1e-10f) {
                    float len = glm::length(jointWorld[i]-a);
                    glm::vec3 nPos = proj + glm::normalize(dirPole) * len;
                    // keep distances to neighbors approximately
                    jointWorld[i] = nPos;
                }
            }
        }
        outIterations = iter+1;
        outError = glm::length(jointWorld.back() - target);
        if (outError <= tolerance) break;
    }
    return true;
}

void WorldChainToLocalRots(const std::vector<glm::mat4>& parentWorld,
                           const std::vector<glm::vec3>& jointWorld,
                           std::vector<glm::quat>& outLocalRots)
{
    const int n = (int)jointWorld.size();
    outLocalRots.assign(n, glm::quat(1,0,0,0));
    for (int i=0;i<n-1;++i) {
        glm::vec3 curDir = glm::vec3(parentWorld[i+1][3]) - glm::vec3(parentWorld[i][3]);
        glm::vec3 newDir = jointWorld[i+1] - jointWorld[i];
        outLocalRots[i] = glm::normalize(FromTo(curDir, newDir));
    }
    // Effector rotation left as identity (aim handled by skin or by end joint orientation)
}

} } }


