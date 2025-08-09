#pragma once
#include "EntityInterop.h"

#ifdef __cplusplus
extern "C" {
#endif

    // --- Component Lifetime ---
    __declspec(dllexport) bool HasComponent(int entityID, const char* componentName);
    __declspec(dllexport) void AddComponent(int entityID, const char* componentName);
    __declspec(dllexport) void RemoveComponent(int entityID, const char* componentName);

    // --- LightComponent ---
    __declspec(dllexport) int GetLightType(int entityID);
    __declspec(dllexport) void SetLightType(int entityID, int type);
    __declspec(dllexport) void GetLightColor(int entityID, float* r, float* g, float* b);
    __declspec(dllexport) void SetLightColor(int entityID, float r, float g, float b);
    __declspec(dllexport) float GetLightIntensity(int entityID);
    __declspec(dllexport) void SetLightIntensity(int entityID, float intensity);

    // --- RigidBodyComponent ---
    __declspec(dllexport) float GetRigidBodyMass(int entityID);
    __declspec(dllexport) void SetRigidBodyMass(int entityID, float mass);
    __declspec(dllexport) bool GetRigidBodyIsKinematic(int entityID);
    __declspec(dllexport) void SetRigidBodyIsKinematic(int entityID, bool isKinematic);
    __declspec(dllexport) void GetRigidBodyLinearVelocity(int entityID, float* x, float* y, float* z);
    __declspec(dllexport) void SetRigidBodyLinearVelocity(int entityID, float x, float y, float z);
    __declspec(dllexport) void GetRigidBodyAngularVelocity(int entityID, float* x, float* y, float* z);
    __declspec(dllexport) void SetRigidBodyAngularVelocity(int entityID, float x, float y, float z);

    // --- BlendShapeComponent ---
    __declspec(dllexport) void SetBlendShapeWeight(int entityID, const char* shapeName, float weight);
    __declspec(dllexport) float GetBlendShapeWeight(int entityID, const char* shapeName);
    __declspec(dllexport) int GetBlendShapeCount(int entityID);
    __declspec(dllexport) const char* GetBlendShapeName(int entityID, int index);

    // --- Animator / AnimationPlayer ---
    __declspec(dllexport) void Animator_SetBool(int entityID, const char* name, bool value);
    __declspec(dllexport) void Animator_SetInt(int entityID, const char* name, int value);
    __declspec(dllexport) void Animator_SetFloat(int entityID, const char* name, float value);
    __declspec(dllexport) void Animator_SetTrigger(int entityID, const char* name);
    __declspec(dllexport) void Animator_ResetTrigger(int entityID, const char* name);

#ifdef __cplusplus
}
#endif
