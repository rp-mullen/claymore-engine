#include "navigation/NavInterop.h"
#include "navigation/Navigation.h"
#include "ecs/Scene.h"
#include "ecs/EntityData.h"
#include "ecs/Components.h"
#include <atomic>

using namespace nav;
using namespace nav::interop;

namespace {
    std::atomic<Fn_Nav_FindPath> g_FindPath{nullptr};
    std::atomic<Fn_Agent_SetDestination> g_SetDest{nullptr};
    std::atomic<Fn_Agent_Stop> g_Stop{nullptr};
    std::atomic<Fn_Agent_Warp> g_Warp{nullptr};
    std::atomic<Fn_Agent_RemainingDist> g_Remain{nullptr};
    std::atomic<Fn_OnPathComplete> g_OnPathComplete{nullptr};
}

void nav::interop::Nav_RegisterManagedCallbacks(Fn_Nav_FindPath findPath,
                                                Fn_Agent_SetDestination setDest,
                                                Fn_Agent_Stop stop,
                                                Fn_Agent_Warp warp,
                                                Fn_Agent_RemainingDist remaining)
{
    g_FindPath.store(findPath);
    g_SetDest.store(setDest);
    g_Stop.store(stop);
    g_Warp.store(warp);
    g_Remain.store(remaining);
}

void nav::interop::Nav_SetOnPathComplete(Fn_OnPathComplete cb)
{
    g_OnPathComplete.store(cb);
}

void nav::interop::FireOnPathComplete(uint64_t managedHandle, bool success)
{
    if (auto fn = g_OnPathComplete.load()) fn(managedHandle, success);
}

// ---------------- Native functions exposed to managed via init table ----------------
static bool Nav_FindPath_Native(EntityID navMeshEntity, glm::vec3 start, glm::vec3 end,
                                const NavAgentParams& p, uint32_t includeFlags, uint32_t excludeFlags,
                                /*out*/ NavPath* outPath)
{
    if (!outPath) return false;
    Scene& scene = Scene::Get();
    NavPath path;
    bool ok = Navigation::Get().FindPath(scene, navMeshEntity, start, end, p, NavFlags{includeFlags}, NavFlags{excludeFlags}, path);
    *outPath = std::move(path);
    return ok;
}

static void Nav_Agent_SetDestination_Native(EntityID agentEntity, glm::vec3 dest)
{
    if (auto* d = Scene::Get().GetEntityData(agentEntity)) {
        if (d->NavAgent) d->NavAgent->SetDestination(dest);
    }
}

static void Nav_Agent_Stop_Native(EntityID agentEntity)
{
    if (auto* d = Scene::Get().GetEntityData(agentEntity)) {
        if (d->NavAgent) d->NavAgent->Stop();
    }
}

static void Nav_Agent_Warp_Native(EntityID agentEntity, glm::vec3 pos)
{
    if (auto* d = Scene::Get().GetEntityData(agentEntity)) {
        if (d->NavAgent) d->NavAgent->Warp(pos, &d->Transform, &Physics::Get(), d->RigidBody.get());
    }
}

static float Nav_Agent_RemainingDist_Native(EntityID agentEntity)
{
    if (auto* d = Scene::Get().GetEntityData(agentEntity)) {
        if (d->NavAgent) {
            glm::vec3 cur = glm::vec3(d->Transform.WorldMatrix[3]);
            return d->NavAgent->RemainingDistance(cur);
        }
    }
    return 0.0f;
}

// Expose raw pointers for host bootstrap
extern "C" void* Get_Nav_FindPath_Ptr() { return (void*)&Nav_FindPath_Native; }
extern "C" void* Get_Nav_Agent_SetDest_Ptr() { return (void*)&Nav_Agent_SetDestination_Native; }
extern "C" void* Get_Nav_Agent_Stop_Ptr() { return (void*)&Nav_Agent_Stop_Native; }
extern "C" void* Get_Nav_Agent_Warp_Ptr() { return (void*)&Nav_Agent_Warp_Native; }
extern "C" void* Get_Nav_Agent_Remaining_Ptr() { return (void*)&Nav_Agent_RemainingDist_Native; }
extern "C" void* Get_Nav_SetOnPathComplete_Ptr() { return (void*)&nav::interop::Nav_SetOnPathComplete; }


