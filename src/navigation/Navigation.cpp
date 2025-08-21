#include "navigation/Navigation.h"
#include "navigation/NavMesh.h"
#include "navigation/NavAgent.h"
#include "navigation/NavQueries.h"
#include "navigation/NavDebugDraw.h"
#include "ecs/Scene.h"
#include "ecs/Components.h"
#include "navigation/NavInterop.h"
#include "physics/Physics.h"

using namespace nav;

Navigation& Navigation::Get()
{
    static Navigation s; return s;
}

void Navigation::SetDebugMask(NavDrawMask mask) { m_DebugMask = mask; debug::SetMask(mask); }
NavDrawMask Navigation::GetDebugMask() const { return m_DebugMask; }

bool Navigation::FindPath(Scene& scene, uint32_t navMeshEntity, const glm::vec3& start, const glm::vec3& end,
                          const NavAgentParams& p, NavFlags include, NavFlags exclude, NavPath& out)
{
    auto* data = scene.GetEntityData(navMeshEntity);
    if (!data || !data->Navigation) return false;
    auto& comp = *data->Navigation; // added to Components.h later
    if (!comp.Runtime && !comp.EnsureRuntimeLoaded()) return false;
    return comp.Runtime->FindPath(start, end, out, p, include, exclude);
}

bool Navigation::Raycast(Scene& scene, uint32_t navMeshEntity, const glm::vec3& start, const glm::vec3& end, float& tHit, glm::vec3& hitNormal)
{
    auto* data = scene.GetEntityData(navMeshEntity);
    if (!data || !data->Navigation) return false;
    auto& comp = *data->Navigation;
    if (!comp.Runtime && !comp.EnsureRuntimeLoaded()) return false;
    return comp.Runtime->Raycast(start, end, tHit, hitNormal);
}

bool Navigation::NearestPoint(Scene& scene, uint32_t navMeshEntity, const glm::vec3& pos, float maxDist, glm::vec3& outOnMesh)
{
    auto* data = scene.GetEntityData(navMeshEntity);
    if (!data || !data->Navigation) return false;
    auto& comp = *data->Navigation;
    if (!comp.Runtime && !comp.EnsureRuntimeLoaded()) return false;
    return comp.Runtime->NearestPoint(pos, maxDist, outOnMesh);
}

void Navigation::Update(Scene& scene, float dt)
{
    // Iterate agents, compute steering along paths
    for (const auto& e : scene.GetEntities()) {
        auto* d = scene.GetEntityData(e.GetID()); if (!d) continue;
        if (!d->NavAgent || !d->NavAgent->Enabled) continue;
        NavAgentComponent& agent = *d->NavAgent;
        ::TransformComponent& tr = d->Transform;

        glm::vec3 position = glm::vec3(tr.WorldMatrix[3]);

        if (agent.HasDestination && !agent.HasPath()) {
            // fetch navmesh comp
            auto* meshOwner = scene.GetEntityData(agent.NavMeshEntity);
            if (meshOwner && meshOwner->Navigation) {
                auto& nav = *meshOwner->Navigation;
                if (nav.Runtime || nav.EnsureRuntimeLoaded()) {
                    NavPath pth; bool ok = nav.Runtime->FindPath(position, agent.Destination, pth, agent.Params, NavFlags{0}, NavFlags{0});
                    agent.CurrentPath = pth; agent.PathCursor = 0; agent.PathRequested = true;
                }
            }
        }

        // follow path
        glm::vec3 desiredVel{0};
        if (agent.HasPath()) {
            // advance cursor if close
            while (agent.PathCursor < agent.CurrentPath.points.size()) {
                if (glm::distance(position, agent.CurrentPath.points[agent.PathCursor]) < agent.Params.radius * 0.5f + 0.05f) agent.PathCursor++; else break;
            }
            if (agent.PathCursor >= agent.CurrentPath.points.size()) {
                // Arrived
                agent.Stop();
                if (agent.ManagedHandle) {
                    nav::interop::FireOnPathComplete(agent.ManagedHandle, true);
                }
            } else {
                glm::vec3 target = agent.CurrentPath.points[agent.PathCursor];
                glm::vec3 to = target - position; float dist = glm::length(to);
                if (dist > 1e-3f) {
                    glm::vec3 dir = to / dist;
                    float speed = glm::min(agent.Params.maxSpeed, dist / glm::max(1e-3f, dt));
                    desiredVel = dir * speed;
                }
            }
        }

        // simple acceleration limit
        glm::vec3 vel = desiredVel; // future: store previous vel per agent and accel-limit

        // apply movement: physics or transform
        if (d->RigidBody && !d->RigidBody->BodyID.IsInvalid()) {
            if (d->RigidBody->IsKinematic) {
                d->RigidBody->LinearVelocity = vel;
            } else {
                ::Physics::Get().SetBodyLinearVelocity(d->RigidBody->BodyID, vel);
            }
        } else {
            tr.Position += vel * dt; tr.TransformDirty = true;
        }

        // debug draw
        debug::DrawPath(agent.CurrentPath, 0);
        debug::DrawAgent(agent, position, vel, 0);
    }

    // Draw navmesh runtime when debug is enabled
    if ((uint32_t)m_DebugMask != 0) {
        for (const auto& e : scene.GetEntities()) {
            auto* d = scene.GetEntityData(e.GetID()); if (!d || !d->Navigation) continue;
            auto& comp = *d->Navigation;
            if (comp.Runtime || comp.EnsureRuntimeLoaded()) {
                debug::DrawRuntime(*comp.Runtime, 0);
            }
        }
    }
}


