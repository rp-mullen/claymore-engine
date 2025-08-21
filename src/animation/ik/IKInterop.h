// IKInterop.h
#pragma once

#include <cstdint>
#include "animation/ik/IKTypes.h"

using EntityID = uint32_t;

namespace cm { namespace animation { namespace ik { namespace interop {

using Fn_IK_SetWeight      = void(*)(EntityID entity, float w);
using Fn_IK_SetTarget      = void(*)(EntityID entity, EntityID target);
using Fn_IK_SetPole        = void(*)(EntityID entity, EntityID pole);
using Fn_IK_SetChain       = void(*)(EntityID entity, const BoneId* ids, int count);
using Fn_IK_GetErrorMeters = float(*)(EntityID entity);

extern "C" void IK_RegisterManagedCallbacks(Fn_IK_SetWeight, Fn_IK_SetTarget, Fn_IK_SetPole, Fn_IK_SetChain, Fn_IK_GetErrorMeters);

} } } }


