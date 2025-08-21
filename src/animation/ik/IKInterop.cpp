// IKInterop.cpp
#include "animation/ik/IKInterop.h"
#include "ecs/Scene.h"
#include "ecs/EntityData.h"
#include <atomic>

using namespace cm::animation::ik;
using namespace cm::animation::ik::interop;

// --------------------------------------------------------------------------------------
// Native-callable implementations that managed side will invoke via function pointers
// Pattern mirrors Navigation interop: expose raw ptr getters consumed by DotNetHost.
// --------------------------------------------------------------------------------------
static inline IKComponent* GetFirstIK(EntityID entity)
{
    auto* d = Scene::Get().GetEntityData(entity);
    if (!d) return nullptr;
    if (d->IKs.empty()) return nullptr;
    return &d->IKs.front();
}

static void IK_SetWeight_Native(EntityID entity, float w)
{
    if (auto* ik = GetFirstIK(entity)) ik->SetWeight(w);
}

static void IK_SetTarget_Native(EntityID entity, EntityID target)
{
    if (auto* ik = GetFirstIK(entity)) ik->SetTarget(target);
}

static void IK_SetPole_Native(EntityID entity, EntityID pole)
{
    if (auto* ik = GetFirstIK(entity)) ik->SetPole(pole);
}

static void IK_SetChain_Native(EntityID entity, const BoneId* ids, int count)
{
    if (count <= 0 || !ids) return;
    if (auto* ik = GetFirstIK(entity)) ik->SetChain(ids, static_cast<size_t>(count));
}

static float IK_GetErrorMeters_Native(EntityID entity)
{
    if (auto* ik = GetFirstIK(entity)) return ik->RuntimeErrorMeters;
    return 0.0f;
}

// --------------------------------------------------------------------------------------
// Optional legacy registration entry (kept for compatibility; currently unused)
// --------------------------------------------------------------------------------------
namespace {
    std::atomic<Fn_IK_SetWeight>      g_SetWeight{nullptr};
    std::atomic<Fn_IK_SetTarget>      g_SetTarget{nullptr};
    std::atomic<Fn_IK_SetPole>        g_SetPole{nullptr};
    std::atomic<Fn_IK_SetChain>       g_SetChain{nullptr};
    std::atomic<Fn_IK_GetErrorMeters> g_GetErr{nullptr};
}

extern "C" void IK_RegisterManagedCallbacks(Fn_IK_SetWeight a,
                                             Fn_IK_SetTarget b,
                                             Fn_IK_SetPole c,
                                             Fn_IK_SetChain d,
                                             Fn_IK_GetErrorMeters e)
{
    g_SetWeight.store(a);
    g_SetTarget.store(b);
    g_SetPole.store(c);
    g_SetChain.store(d);
    g_GetErr.store(e);
}

// --------------------------------------------------------------------------------------
// Raw pointer getters consumed by DotNetHost to bootstrap managed interop
// --------------------------------------------------------------------------------------
extern "C" void* Get_IK_SetWeight_Ptr()      { return (void*)&IK_SetWeight_Native; }
extern "C" void* Get_IK_SetTarget_Ptr()      { return (void*)&IK_SetTarget_Native; }
extern "C" void* Get_IK_SetPole_Ptr()        { return (void*)&IK_SetPole_Native; }
extern "C" void* Get_IK_SetChain_Ptr()       { return (void*)&IK_SetChain_Native; }
extern "C" void* Get_IK_GetErrorMeters_Ptr() { return (void*)&IK_GetErrorMeters_Native; }

