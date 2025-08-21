// IKDebugDraw.cpp
#include "animation/ik/IKDebugDraw.h"
#include "rendering/Renderer.h"

namespace cm { namespace animation { namespace ik {

void DrawChain(const DebugChainViz& viz, uint16_t viewId) {
    if (viz.jointWorld.size() >= 2) {
        for (size_t i=1;i<viz.jointWorld.size();++i) {
            Renderer::Get().DrawDebugRay(viz.jointWorld[i-1], viz.jointWorld[i]-viz.jointWorld[i-1], 1.0f);
        }
    }
    // Target line from effector
    if (!viz.jointWorld.empty()) {
        glm::vec3 eff = viz.jointWorld.back();
        Renderer::Get().DrawDebugRay(eff, viz.targetWorld - eff, 1.0f);
    }
    if (viz.hasPole && viz.jointWorld.size()>=1) {
        Renderer::Get().DrawDebugRay(viz.jointWorld[0], viz.poleWorld - viz.jointWorld[0], 1.0f);
    }
}

} } }


