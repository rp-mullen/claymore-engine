#include "navigation/NavAgent.h"
#include "ecs/Components.h"
#include "physics/Physics.h"

using namespace nav;

void NavAgentComponent::SetDestination(const glm::vec3& dest)
{
    Destination = dest;
    HasDestination = true;
    PathRequested = false; // schedule new path on next update
}

void NavAgentComponent::Stop()
{
    HasDestination = false;
    CurrentPath.points.clear();
    CurrentPath.valid = false;
    PathCursor = 0;
}

void NavAgentComponent::Warp(const glm::vec3& pos, ::TransformComponent* transform, ::Physics* physics, ::RigidBodyComponent* rb)
{
    if (rb && !rb->BodyID.IsInvalid() && rb->IsKinematic == false) {
        // Teleport dynamic body by setting transform directly
        physics->SetBodyTransform(rb->BodyID, pos, transform ? transform->Rotation : glm::vec3(0));
    }
    if (transform) {
        transform->Position = pos;
        transform->TransformDirty = true;
    }
    Stop();
}

float NavAgentComponent::RemainingDistance(const glm::vec3& currentPos) const
{
    if (!HasPath()) return 0.0f;
    float sum = 0.0f;
    glm::vec3 prev = currentPos;
    for (size_t i = PathCursor; i < CurrentPath.points.size(); ++i) {
        sum += glm::length(CurrentPath.points[i] - prev);
        prev = CurrentPath.points[i];
    }
    return sum;
}


