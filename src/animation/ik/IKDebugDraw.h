// IKDebugDraw.h
#pragma once

#include <vector>
#include <glm/glm.hpp>

namespace cm { namespace animation { namespace ik {

struct DebugChainViz {
    std::vector<glm::vec3> jointWorld;
    glm::vec3 targetWorld{0.0f};
    bool hasPole = false; glm::vec3 poleWorld{0.0f};
    float error = 0.0f; int iterations = 0;
};

void DrawChain(const DebugChainViz& viz, uint16_t viewId = 0);

} } }


