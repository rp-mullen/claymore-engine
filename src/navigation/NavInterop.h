#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include "navigation/NavTypes.h"

using EntityID = uint32_t;

namespace nav::interop
{
    using Fn_Nav_FindPath = bool(*)(EntityID navMeshEntity, glm::vec3 start, glm::vec3 end,
                                    const NavAgentParams&, uint32_t includeFlags, uint32_t excludeFlags,
                                    /*out*/ NavPath* outPath);

    using Fn_Agent_SetDestination = void(*)(EntityID agentEntity, glm::vec3 dest);
    using Fn_Agent_Stop           = void(*)(EntityID agentEntity);
    using Fn_Agent_Warp           = void(*)(EntityID agentEntity, glm::vec3 pos);
    using Fn_Agent_RemainingDist  = float(*)(EntityID agentEntity);

    using Fn_OnPathComplete = void(*)(uint64_t managedAgentHandle, bool success);

    void Nav_RegisterManagedCallbacks(Fn_Nav_FindPath findPath,
                                      Fn_Agent_SetDestination setDest,
                                      Fn_Agent_Stop stop,
                                      Fn_Agent_Warp warp,
                                      Fn_Agent_RemainingDist remaining);

    void Nav_SetOnPathComplete(Fn_OnPathComplete cb);

    // Helpers for native to fire callbacks
    void FireOnPathComplete(uint64_t managedHandle, bool success);
}


