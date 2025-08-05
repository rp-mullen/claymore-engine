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

GetEntityPosition_fn GetEntityPositionPtr = &GetEntityPosition;
SetEntityPosition_fn SetEntityPositionPtr = &SetEntityPosition;
FindEntityByName_fn  FindEntityByNamePtr = &FindEntityByName;
CreateEntity_fn  CreateEntityPtr = &CreateEntity;
DestroyEntity_fn DestroyEntityPtr = &DestroyEntity;
GetEntityRotation_fn GetEntityRotationPtr = &GetEntityRotation;
SetEntityRotation_fn SetEntityRotationPtr = &SetEntityRotation;
GetEntityScale_fn    GetEntityScalePtr    = &GetEntityScale;
SetEntityScale_fn    SetEntityScalePtr    = &SetEntityScale;
SetLinearVelocity_fn SetLinearVelocityPtr = &SetLinearVelocity;
SetAngularVelocity_fn SetAngularVelocityPtr = &SetAngularVelocity;
SetLightColor_fn     SetLightColorPtr      = &SetLightColor;
SetLightIntensity_fn SetLightIntensityPtr  = &SetLightIntensity;
SetBlendShapeWeight_fn SetBlendShapeWeightPtr = &SetBlendShapeWeight;

extern "C"
{
    __declspec(dllexport) void GetEntityPosition(int entityID, float* outX, float* outY, float* outZ)
    {
        auto pos = Scene::Get().GetEntityData(entityID)->Transform.Position;
        *outX = pos.x;
        *outY = pos.y;
        *outZ = pos.z;
    }

    __declspec(dllexport) void SetEntityPosition(int entityID, float x, float y, float z)
    {
        auto data = Scene::Get().GetEntityData(entityID);
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
        Scene::Get().RemoveEntity(entityID);
    }

    // Rotation (Euler degrees)
    __declspec(dllexport) void GetEntityRotation(int entityID, float* outX, float* outY, float* outZ)
    {
        auto rot = Scene::Get().GetEntityData(entityID)->Transform.Rotation;
        *outX = rot.x; *outY = rot.y; *outZ = rot.z;
    }

    __declspec(dllexport) void SetEntityRotation(int entityID, float x, float y, float z)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(!data) return;
        data->Transform.Rotation = glm::vec3(x, y, z);
        Scene::Get().MarkTransformDirty(entityID);
    }

    // Scale
    __declspec(dllexport) void GetEntityScale(int entityID, float* outX, float* outY, float* outZ)
    {
        auto scale = Scene::Get().GetEntityData(entityID)->Transform.Scale;
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

    // Lighting
    __declspec(dllexport) void SetLightColor(int entityID, float r, float g, float b)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(data && data->Light)
            data->Light->Color = glm::vec3(r, g, b);
    }

    __declspec(dllexport) void SetLightIntensity(int entityID, float intensity)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(data && data->Light)
            data->Light->Intensity = intensity;
    }

    // Blendshape weight
    __declspec(dllexport) void SetBlendShapeWeight(int entityID, const char* shapeName, float weight)
    {
        auto* data = Scene::Get().GetEntityData(entityID);
        if(!data || !data->BlendShapes || !shapeName) return;
        for(auto& shape : data->BlendShapes->Shapes)
        {
            if(shape.Name == shapeName)
            {
                shape.Weight = weight;
                data->BlendShapes->Dirty = true;
                break;
            }
        }
    }
}

