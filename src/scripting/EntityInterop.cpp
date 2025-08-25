#include "EntityInterop.h"
#include "DotNetHost.h"
#include "ecs/Scene.h"
#include "physics/Physics.h"
#include <glm/glm.hpp>
#include <cstring>
#include <algorithm>
#include "ecs/AnimationComponents.h"
#include "rendering/PBRMaterial.h"
#include "ecs/Components.h"

#include "ComponentInterop.h" 

GetEntityPosition_fn GetEntityPositionPtr = &GetEntityPosition;
SetEntityPosition_fn SetEntityPositionPtr = &SetEntityPosition;
FindEntityByName_fn  FindEntityByNamePtr = &FindEntityByName;
GetEntities_fn      GetEntitiesPtr = &GetEntities;
GetEntityCount_fn   GetEntityCountPtr = &GetEntityCount;
CreateEntity_fn  CreateEntityPtr = &CreateEntity;
DestroyEntity_fn DestroyEntityPtr = &DestroyEntity;
GetEntityByID_fn GetEntityByIDPtr = &GetEntityByID;
GetEntityRotation_fn GetEntityRotationPtr = &GetEntityRotation;
SetEntityRotation_fn SetEntityRotationPtr = &SetEntityRotation;
GetEntityRotationQuat_fn GetEntityRotationQuatPtr = &GetEntityRotationQuat;
SetEntityRotationQuat_fn SetEntityRotationQuatPtr = &SetEntityRotationQuat;
GetEntityScale_fn    GetEntityScalePtr    = &GetEntityScale;
SetEntityScale_fn    SetEntityScalePtr    = &SetEntityScale;
SetLinearVelocity_fn SetLinearVelocityPtr = &SetLinearVelocity;
SetAngularVelocity_fn SetAngularVelocityPtr = &SetAngularVelocity;

extern "C"
{
    __declspec(dllexport) void GetEntityPosition(int entityID, float* outX, float* outY, float* outZ)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(!data)
        {
            *outX = *outY = *outZ = 0.0f;
            return;
        }
        auto pos = data->Transform.Position;
        *outX = pos.x;
        *outY = pos.y;
        *outZ = pos.z;
    }

    __declspec(dllexport) void SetEntityPosition(int entityID, float x, float y, float z)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(!data) return;
        data->Transform.Position = glm::vec3(x, y, z);
        Scene::Get().MarkTransformDirty(entityID);
    }

    __declspec(dllexport) int FindEntityByName(const char* name)
    {
        for (const Entity& e : Scene::Get().GetEntities())
            if (e.GetName() == name)
                return e.GetID();
        return -1;
    }

    // ----------------------------------------------------------------------
    // Additional Interop Methods
    // ----------------------------------------------------------------------

    __declspec(dllexport) int CreateEntity(const char* name)
    {
        Entity entity = Scene::Get().CreateEntity(name ? name : "Entity");
        return entity.GetID();
    }

    __declspec(dllexport) void DestroyEntity(int entityID)
    {
       // Defer deletion to avoid mid-frame invalidation
       Scene::Get().QueueRemoveEntity(entityID);
    }

    __declspec(dllexport) int GetEntityByID(int entityID)
    {
        Entity entity = Scene::Get().FindEntityByID(entityID);
        return entity.GetID();
    }

    __declspec(dllexport) int* GetEntities()
       {
       static std::vector<int> s_EntityIDs;
       const auto& entities = Scene::Get().GetEntities();
       s_EntityIDs.resize(entities.size());
       for (size_t i = 0; i < entities.size(); i++)
          s_EntityIDs[i] = entities[i].GetID();
       return s_EntityIDs.data();
       }

    __declspec(dllexport) int GetEntityCount()
    {
        return (int)Scene::Get().GetEntities().size();
    }

    // Rotation (Euler degrees)
    __declspec(dllexport) void GetEntityRotation(int entityID, float* outX, float* outY, float* outZ)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(!data){ *outX = *outY = *outZ = 0.0f; return; }
        auto rot = data->Transform.Rotation;
        *outX = rot.x; *outY = rot.y; *outZ = rot.z;
    }

    __declspec(dllexport) void SetEntityRotation(int entityID, float x, float y, float z)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(!data) return;
        data->Transform.Rotation = glm::vec3(x, y, z);
        // Keep quaternion in sync for runtime consumers
        glm::mat4 m = glm::yawPitchRoll(glm::radians(y), glm::radians(x), glm::radians(z));
        data->Transform.RotationQ = glm::normalize(glm::quat_cast(m));
        data->Transform.UseQuatRotation = false;
        Scene::Get().MarkTransformDirty(entityID);
    }

    __declspec(dllexport) void GetEntityRotationQuat(int entityID, float* outX, float* outY, float* outZ, float* outW)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(!data){ *outX = *outY = *outZ = 0.0f; if(outW) *outW = 1.0f; return; }
        auto q = glm::normalize(data->Transform.RotationQ);
        *outX = q.x; *outY = q.y; *outZ = q.z; if(outW) *outW = q.w;
    }

    __declspec(dllexport) void SetEntityRotationQuat(int entityID, float x, float y, float z, float w)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(!data) return;
        glm::quat q = glm::normalize(glm::quat(w, x, y, z));
        data->Transform.RotationQ = q;
        // Also update Euler for inspector display so UI reflects runtime changes
        glm::vec3 eulerRad = glm::eulerAngles(q); // returns radians (XYZ order)
        data->Transform.Rotation = glm::degrees(eulerRad);
        data->Transform.UseQuatRotation = true;
        Scene::Get().MarkTransformDirty(entityID);
    }

    // Scale
    __declspec(dllexport) void GetEntityScale(int entityID, float* outX, float* outY, float* outZ)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(!data){ *outX = *outY = *outZ = 0.0f; return; }
        auto scale = data->Transform.Scale;
        *outX = scale.x; *outY = scale.y; *outZ = scale.z;
    }

    __declspec(dllexport) void SetEntityScale(int entityID, float x, float y, float z)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(!data) return;
        data->Transform.Scale = glm::vec3(x, y, z);
        Scene::Get().MarkTransformDirty(entityID);
    }

    // Physics velocity setters
    __declspec(dllexport) void SetLinearVelocity(int entityID, float x, float y, float z)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(!data || !data->RigidBody) return;
        JPH::BodyID bodyID = data->RigidBody->BodyID;
        if(bodyID.IsInvalid()) return;
        Physics::SetBodyLinearVelocity(bodyID, glm::vec3(x, y, z));
    }

    __declspec(dllexport) void SetAngularVelocity(int entityID, float x, float y, float z)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(!data || !data->RigidBody) return;
        JPH::BodyID bodyID = data->RigidBody->BodyID;
        if(bodyID.IsInvalid()) return;
        Physics::SetBodyAngularVelocity(bodyID, glm::vec3(x, y, z));
    }

}

