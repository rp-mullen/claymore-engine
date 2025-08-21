#pragma once

#include <glm/glm.hpp>
#include <memory>
#include "navigation/NavTypes.h"
#include "ecs/Components.h"
#include "physics/Physics.h"

using EntityID = uint32_t;

namespace nav
{
    struct NavMeshComponent;

    struct NavAgentComponent
    {
        bool Enabled = true;
        EntityID NavMeshEntity = 0;
        NavAgentParams Params;
        glm::vec3 Destination{ 0 };
        float ArriveThreshold = 0.15f;
        float RepathInterval = 0.5f;
        bool AutoRepath = true;
        float AvoidanceRadiusMul = 1.2f;

        // Runtime
        NavPath CurrentPath;
        size_t PathCursor = 0;
        float RepathTimer = 0.0f;
        bool HasDestination = false;
        bool PathRequested = false;
        uint64_t ManagedHandle = 0;

        // Methods
        void SetDestination(const glm::vec3& dest);
        void Stop();
        void Warp(const glm::vec3& pos, TransformComponent* transform, Physics* physics, RigidBodyComponent* rb);
        bool HasPath() const { return CurrentPath.valid && !CurrentPath.points.empty(); }
        float RemainingDistance(const glm::vec3& currentPos) const;
    };
}


