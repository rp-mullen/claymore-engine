#include "ComponentInterop.h"
#include "ecs/Scene.h"
#include "ecs/Components.h"
#include <string>
#include <iostream>
#include <cstring>
#include "DotNetHost.h"

// --- Global Function Pointers ---
HasComponent_fn HasComponentPtr = &HasComponent;
AddComponent_fn AddComponentPtr = &AddComponent;
RemoveComponent_fn RemoveComponentPtr = &RemoveComponent;
GetLightType_fn GetLightTypePtr = &GetLightType;
SetLightType_fn SetLightTypePtr = &SetLightType;
GetLightColor_fn GetLightColorPtr = &GetLightColor;
SetLightColor_fn SetLightColorPtr = &SetLightColor;
GetLightIntensity_fn GetLightIntensityPtr = &GetLightIntensity;
SetLightIntensity_fn SetLightIntensityPtr = &SetLightIntensity;
GetRigidBodyMass_fn GetRigidBodyMassPtr = &GetRigidBodyMass;
SetRigidBodyMass_fn SetRigidBodyMassPtr = &SetRigidBodyMass;
GetRigidBodyIsKinematic_fn GetRigidBodyIsKinematicPtr = &GetRigidBodyIsKinematic;
SetRigidBodyIsKinematic_fn SetRigidBodyIsKinematicPtr = &SetRigidBodyIsKinematic;
GetRigidBodyLinearVelocity_fn GetRigidBodyLinearVelocityPtr = &GetRigidBodyLinearVelocity;
SetRigidBodyLinearVelocity_fn SetRigidBodyLinearVelocityPtr = &SetRigidBodyLinearVelocity;
GetRigidBodyAngularVelocity_fn GetRigidBodyAngularVelocityPtr = &GetRigidBodyAngularVelocity;
SetRigidBodyAngularVelocity_fn SetRigidBodyAngularVelocityPtr = &SetRigidBodyAngularVelocity;
SetBlendShapeWeight_fn SetBlendShapeWeightPtr = &SetBlendShapeWeight;
GetBlendShapeWeight_fn GetBlendShapeWeightPtr = &GetBlendShapeWeight;
GetBlendShapeCount_fn GetBlendShapeCountPtr = &GetBlendShapeCount;
GetBlendShapeName_fn GetBlendShapeNamePtr = &GetBlendShapeName;

// Helper to get entity data, returns nullptr if not found
static EntityData* GetEntityDataHelper(int entityID) {
    return Scene::Get().GetEntityData(entityID);
}

// --- Component Lifetime ---

bool HasComponent(int entityID, const char* componentName) {
    EntityData* data = GetEntityDataHelper(entityID);
    if (!data) return false;

    if (strcmp(componentName, "LightComponent") == 0) return data->Light != nullptr;
    if (strcmp(componentName, "RigidBodyComponent") == 0) return data->RigidBody != nullptr;
    if (strcmp(componentName, "MeshComponent") == 0) return data->Mesh != nullptr;
    // Add other components here...

    return false;
}

void AddComponent(int entityID, const char* componentName) {
    EntityData* data = GetEntityDataHelper(entityID);
    if (!data) return;

    if (strcmp(componentName, "LightComponent") == 0 && !data->Light) {
        data->Light = new LightComponent();
    } else if (strcmp(componentName, "RigidBodyComponent") == 0 && !data->RigidBody) {
        data->RigidBody = new RigidBodyComponent();
        if(data->Collider) // If collider exists, create physics body
        {
            Scene::Get().CreatePhysicsBody(entityID, data->Transform, *data->Collider);
        }
    }
    // Add other components here...
}

void RemoveComponent(int entityID, const char* componentName) {
    EntityData* data = GetEntityDataHelper(entityID);
    if (!data) return;

    if (strcmp(componentName, "LightComponent") == 0) {
        delete data->Light;
        data->Light = nullptr;
    } else if (strcmp(componentName, "RigidBodyComponent") == 0) {
        Scene::Get().DestroyPhysicsBody(entityID);
        delete data->RigidBody;
        data->RigidBody = nullptr;
    }
    // Add other components here...
}

// --- LightComponent ---

int GetLightType(int entityID) {
    EntityData* data = GetEntityDataHelper(entityID);
    return (data && data->Light) ? static_cast<int>(data->Light->Type) : 0;
}

void SetLightType(int entityID, int type) {
    EntityData* data = GetEntityDataHelper(entityID);
    if (data && data->Light) data->Light->Type = static_cast<LightType>(type);
}

void GetLightColor(int entityID, float* r, float* g, float* b) {
    EntityData* data = GetEntityDataHelper(entityID);
    if (data && data->Light) {
        *r = data->Light->Color.r;
        *g = data->Light->Color.g;
        *b = data->Light->Color.b;
    }
}

void SetLightColor(int entityID, float r, float g, float b) {
    EntityData* data = GetEntityDataHelper(entityID);
    if (data && data->Light) data->Light->Color = {r, g, b};
}

float GetLightIntensity(int entityID) {
    EntityData* data = GetEntityDataHelper(entityID);
    return (data && data->Light) ? data->Light->Intensity : 0.0f;
}

void SetLightIntensity(int entityID, float intensity) {
    EntityData* data = GetEntityDataHelper(entityID);
    if (data && data->Light) data->Light->Intensity = intensity;
}

// --- RigidBodyComponent ---

float GetRigidBodyMass(int entityID) {
    EntityData* data = GetEntityDataHelper(entityID);
    return (data && data->RigidBody) ? data->RigidBody->Mass : 0.0f;
}

void SetRigidBodyMass(int entityID, float mass) {
    EntityData* data = GetEntityDataHelper(entityID);
    if (data && data->RigidBody) {
        data->RigidBody->Mass = mass;
        // Jolt needs to be updated if mass changes while running
    }
}

bool GetRigidBodyIsKinematic(int entityID) {
    EntityData* data = GetEntityDataHelper(entityID);
    return (data && data->RigidBody) ? data->RigidBody->IsKinematic : false;
}

void SetRigidBodyIsKinematic(int entityID, bool isKinematic) {
    EntityData* data = GetEntityDataHelper(entityID);
    if (data && data->RigidBody) {
        data->RigidBody->IsKinematic = isKinematic;
        // Jolt needs to be updated if this changes while running
    }
}

void GetRigidBodyLinearVelocity(int entityID, float* x, float* y, float* z)
{
    EntityData* data = GetEntityDataHelper(entityID);
    if (data && data->RigidBody)
    {
        JPH::BodyID bodyID = data->RigidBody->BodyID;
        if (!bodyID.IsInvalid())
        {
            auto vel = Physics::GetBodyInterface().GetLinearVelocity(bodyID);
            *x = vel.GetX();
            *y = vel.GetY();
            *z = vel.GetZ();
            return;
        }
    }
    *x = *y = *z = 0;
}

void SetRigidBodyLinearVelocity(int entityID, float x, float y, float z)
{
    SetLinearVelocity(entityID, x, y, z); // From EntityInterop
}

void GetRigidBodyAngularVelocity(int entityID, float* x, float* y, float* z)
{
    EntityData* data = GetEntityDataHelper(entityID);
    if (data && data->RigidBody)
    {
        JPH::BodyID bodyID = data->RigidBody->BodyID;
        if (!bodyID.IsInvalid())
        {
            auto vel = Physics::GetBodyInterface().GetAngularVelocity(bodyID);
            *x = vel.GetX();
            *y = vel.GetY();
            *z = vel.GetZ();
            return;
        }
    }
    *x = *y = *z = 0;
}

void SetRigidBodyAngularVelocity(int entityID, float x, float y, float z)
{
    SetAngularVelocity(entityID, x, y, z); // From EntityInterop
}

// --- BlendShapeComponent ---

void SetBlendShapeWeight(int entityID, const char* shapeName, float weight)
{
    EntityData* data = GetEntityDataHelper(entityID);
    if (!data || !data->BlendShapes || !shapeName) return;
    for (auto& shape : data->BlendShapes->Shapes)
    {
        if (shape.Name == shapeName)
        {
            shape.Weight = weight;
            data->BlendShapes->Dirty = true;
            break;
        }
    }
}

float GetBlendShapeWeight(int entityID, const char* shapeName)
{
    EntityData* data = GetEntityDataHelper(entityID);
    if (!data || !data->BlendShapes || !shapeName) return 0.0f;
    for (const auto& shape : data->BlendShapes->Shapes)
    {
        if (shape.Name == shapeName)
        {
            return shape.Weight;
        }
    }
    return 0.0f;
}

int GetBlendShapeCount(int entityID)
{
    EntityData* data = GetEntityDataHelper(entityID);
    return (data && data->BlendShapes) ? data->BlendShapes->Shapes.size() : 0;
}

const char* GetBlendShapeName(int entityID, int index)
{
    EntityData* data = GetEntityDataHelper(entityID);
    if (!data || !data->BlendShapes || index < 0 || index >= data->BlendShapes->Shapes.size())
    {
        return nullptr;
    }
    return data->BlendShapes->Shapes[index].Name.c_str();
}
