#pragma once

#include <cstdint>
#include <atomic>
#include <functional>
#include <memory>

namespace nav { struct NavMeshComponent; }
class Scene;

namespace nav::jobs
{
    struct BakeJobState
    {
        std::atomic<float> progress{0.0f};
        std::atomic<bool> cancel{false};
    };

    // Fire-and-forget; attaches to component state
    void SubmitBake(NavMeshComponent* comp, Scene* scene);
}


