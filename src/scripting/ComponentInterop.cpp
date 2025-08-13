#include "ComponentInterop.h"
#include "ecs/Scene.h"
#include "ecs/Components.h"
#include <string>
#include <iostream>
#include <cstring>
#include "DotNetHost.h"
#include "animation/AnimationPlayerComponent.h"
#include "animation/AnimatorRuntime.h"

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
    if (strcmp(componentName, "Animator") == 0 || strcmp(componentName, "AnimationPlayerComponent") == 0) return data->AnimationPlayer != nullptr;
    // Add other components here...

    return false;
}

void AddComponent(int entityID, const char* componentName) {
    EntityData* data = GetEntityDataHelper(entityID);
    if (!data) return;

    if (strcmp(componentName, "LightComponent") == 0 && !data->Light) {
        std::cout << "[Interop] AddComponent Light -> Entity " << entityID << "\n";
        data->Light = new LightComponent();
    } else if (strcmp(componentName, "RigidBodyComponent") == 0 && !data->RigidBody) {
        data->RigidBody = new RigidBodyComponent();
        if(data->Collider) // If collider exists, create physics body
        {
            Scene::Get().CreatePhysicsBody(entityID, data->Transform, *data->Collider);
        }
    } else if ((strcmp(componentName, "Animator") == 0 || strcmp(componentName, "AnimationPlayerComponent") == 0) && !data->AnimationPlayer) {
        data->AnimationPlayer = new cm::animation::AnimationPlayerComponent();
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
    } else if (strcmp(componentName, "Animator") == 0 || strcmp(componentName, "AnimationPlayerComponent") == 0) {
        delete data->AnimationPlayer;
        data->AnimationPlayer = nullptr;
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
    if (data && data->Light) {
        data->Light->Type = static_cast<LightType>(type);
        std::cout << "[Interop] SetLightType entity=" << entityID << " type=" << type << "\n";
    }
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
    if (data && data->Light) {
        data->Light->Color = {r, g, b};
        std::cout << "[Interop] SetLightColor entity=" << entityID << " color=(" << r << "," << g << "," << b << ")\n";
    }
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

// --- Animator parameter setters ---
static cm::animation::AnimationPlayerComponent* GetAnimatorFor(int entityID)
{
    EntityData* data = GetEntityDataHelper(entityID);
    return (data ? data->AnimationPlayer : nullptr);
}

void Animator_SetBool(int entityID, const char* name, bool value)
{
    if (!name) return;
    if (auto* ap = GetAnimatorFor(entityID)) {
        ap->AnimatorInstance.Blackboard().Bools[name] = value;
    }
}

void Animator_SetInt(int entityID, const char* name, int value)
{
    if (!name) return;
    if (auto* ap = GetAnimatorFor(entityID)) {
        ap->AnimatorInstance.Blackboard().Ints[name] = value;
    }
}

void Animator_SetFloat(int entityID, const char* name, float value)
{
    if (!name) return;
    if (auto* ap = GetAnimatorFor(entityID)) {
        ap->AnimatorInstance.Blackboard().Floats[name] = value;
    }
}

void Animator_SetTrigger(int entityID, const char* name)
{
    if (!name) return;
    if (auto* ap = GetAnimatorFor(entityID)) {
        ap->AnimatorInstance.Blackboard().Triggers[name] = true;
    }
}

void Animator_ResetTrigger(int entityID, const char* name)
{
    if (!name) return;
    if (auto* ap = GetAnimatorFor(entityID)) {
        ap->AnimatorInstance.Blackboard().Triggers[name] = false;
    }
}

// --- AnimationPlayer (single-clip mode) controls ---
void AnimationPlayer_Play(int entityID)
{
    if (auto* ap = GetAnimatorFor(entityID)) {
        ap->AnimatorMode = cm::animation::AnimationPlayerComponent::Mode::AnimationPlayerAnimated;
        ap->IsPlaying = true;
        if (!ap->ActiveStates.empty()) ap->ActiveStates.front().Time = 0.0f;
    }
}

void AnimationPlayer_Stop(int entityID)
{
    if (auto* ap = GetAnimatorFor(entityID)) {
        ap->IsPlaying = false;
    }
}

bool AnimationPlayer_IsPlaying(int entityID)
{
    if (auto* ap = GetAnimatorFor(entityID)) return ap->IsPlaying;
    return false;
}

void AnimationPlayer_SetLoop(int entityID, bool loop)
{
    if (auto* ap = GetAnimatorFor(entityID)) {
        if (ap->ActiveStates.empty()) ap->ActiveStates.push_back({});
        ap->ActiveStates.front().Loop = loop;
    }
}

void AnimationPlayer_SetSpeed(int entityID, float speed)
{
    if (auto* ap = GetAnimatorFor(entityID)) ap->PlaybackSpeed = speed;
}

const char* AnimationPlayer_GetCurrentClipName(int entityID)
{
    if (auto* ap = GetAnimatorFor(entityID)) return ap->Debug_CurrentAnimationName.c_str();
    return "";
}

const char* Animator_GetCurrentStateName(int entityID)
{
    if (auto* ap = GetAnimatorFor(entityID)) return ap->Debug_CurrentControllerStateName.c_str();
    return "";
}

bool Animator_IsPlaying(int entityID)
{
    if (auto* ap = GetAnimatorFor(entityID)) {
        if (ap->AnimatorMode == cm::animation::AnimationPlayerComponent::Mode::ControllerAnimated)
            return true; // controller is always advancing
        return ap->IsPlaying;
    }
    return false;
}