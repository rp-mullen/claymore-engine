#pragma once
#include <string>
#include "ecs/Entity.h"
#include <vector>
#include <filesystem>

bool LoadHostFxr();
bool LoadDotnetRuntime(const std::wstring& assemblyPath, const std::wstring& typeName, const std::wstring& methodName);


#include "scripting/ComponentInterop.h"
#include "scripting/EntityInterop.h"

// Script interop function pointer types
using Script_Create_fn = void* (*)(const char* className);
using Script_OnCreate_fn = void (*)(void* handle, int entityID);
using Script_OnUpdate_fn = void (*)(void* handle, float dt);
using Script_Invoke_fn = void (*)(void* handle, const char* methodName);
using Script_Destroy_fn = void (*)(void* handle);
using ReloadScripts_fn = int (*)(const wchar_t*);

// These are resolved at runtime
extern Script_Create_fn g_Script_Create;
extern Script_OnCreate_fn g_Script_OnCreate;
extern Script_OnUpdate_fn g_Script_OnUpdate;
extern Script_Invoke_fn g_Script_Invoke;
extern Script_Destroy_fn g_Script_Destroy;
extern ReloadScripts_fn g_ReloadScripts;
// Creates and initializes the managed script instance for an entity
void CallOnCreate(void* instance, int entityID);
void CallOnUpdate(void* instance, float dt);
void ReloadScripts();
void SetupEntityInterop(std::filesystem::path fullPath);
void SetupInputInterop(std::filesystem::path fullPath);
void SetupReflectionInterop(std::filesystem::path fullPath);

// SetField pointer
#include "scripting/ScriptReflectionInterop.h"

// Creates a managed script instance (returns GCHandle pointer held as void*)
void* CreateScriptInstance(const std::string& className);

// Function pointer types
using GetEntityPosition_fn     = void(*)(int entityID, float* outX, float* outY, float* outZ);
using SetEntityPosition_fn     = void(*)(int entityID, float x, float y, float z);
using FindEntityByName_fn      = int (*)(const char* name);
// Entity management
using CreateEntity_fn          = int (*)(const char* name);
using DestroyEntity_fn         = void(*)(int entityID);
// Rotation & Scale
using GetEntityRotation_fn     = void(*)(int entityID, float* outX, float* outY, float* outZ);
using SetEntityRotation_fn     = void(*)(int entityID, float x, float y, float z);
using GetEntityRotationQuat_fn = void(*)(int entityID, float* outX, float* outY, float* outZ, float* outW);
using SetEntityRotationQuat_fn = void(*)(int entityID, float x, float y, float z, float w);
using GetEntityScale_fn        = void(*)(int entityID, float* outX, float* outY, float* outZ);
using SetEntityScale_fn        = void(*)(int entityID, float x, float y, float z);
// Physics
using SetLinearVelocity_fn     = void(*)(int entityID, float x, float y, float z);
using SetAngularVelocity_fn    = void(*)(int entityID, float x, float y, float z);

// --- Component Interop Function Pointer Types ---
using HasComponent_fn = bool (*)(int, const char*);
using AddComponent_fn = void (*)(int, const char*);
using RemoveComponent_fn = void (*)(int, const char*);
using GetLightType_fn = int (*)(int);
using SetLightType_fn = void (*)(int, int);
using GetLightColor_fn = void (*)(int, float*, float*, float*);
using SetLightColor_fn = void (*)(int, float, float, float);
using GetLightIntensity_fn = float (*)(int);
using SetLightIntensity_fn = void (*)(int, float);
using GetRigidBodyMass_fn = float (*)(int);
using SetRigidBodyMass_fn = void (*)(int, float);
using GetRigidBodyIsKinematic_fn = bool (*)(int);
using SetRigidBodyIsKinematic_fn = void (*)(int, bool);
using GetRigidBodyLinearVelocity_fn = void (*)(int, float*, float*, float*);
using SetRigidBodyLinearVelocity_fn = void (*)(int, float, float, float);
using GetRigidBodyAngularVelocity_fn = void (*)(int, float*, float*, float*);
using SetRigidBodyAngularVelocity_fn = void (*)(int, float, float, float);
using SetBlendShapeWeight_fn = void(*)(int, const char*, float);
using GetBlendShapeWeight_fn = float(*)(int, const char*);
using GetBlendShapeCount_fn = int(*)(int);
using GetBlendShapeName_fn = const char*(*)(int, int);
using GetEntityByID_fn = int (*)(int);

// SyncContext controls
using FlushSyncContext_fn      = void(__stdcall*)();
using ClearSyncContext_fn      = void(__stdcall*)();
using RegisterScriptCallbackFn = void(*)(const char*);

using InstallSyncContext_fn = void(__stdcall*)();
using EnsureInstalled_fn = void(__stdcall*)();

// Pointers to assign in Claymore main.cpp or initialization
extern GetEntityPosition_fn   GetEntityPositionPtr;
extern SetEntityPosition_fn   SetEntityPositionPtr;
extern FindEntityByName_fn    FindEntityByNamePtr;
extern CreateEntity_fn        CreateEntityPtr;
extern DestroyEntity_fn       DestroyEntityPtr;
extern GetEntityByID_fn       GetEntityByIDPtr;
extern GetEntityRotation_fn   GetEntityRotationPtr;
extern SetEntityRotation_fn   SetEntityRotationPtr;
extern GetEntityRotationQuat_fn GetEntityRotationQuatPtr;
extern SetEntityRotationQuat_fn SetEntityRotationQuatPtr;
extern GetEntityScale_fn      GetEntityScalePtr;
extern SetEntityScale_fn      SetEntityScalePtr;
extern SetLinearVelocity_fn   SetLinearVelocityPtr;
extern SetAngularVelocity_fn  SetAngularVelocityPtr;

// --- Component Interop Function Pointers ---
extern HasComponent_fn HasComponentPtr;
extern AddComponent_fn AddComponentPtr;
extern RemoveComponent_fn RemoveComponentPtr;
extern GetLightType_fn GetLightTypePtr;
extern SetLightType_fn SetLightTypePtr;
extern GetLightColor_fn GetLightColorPtr;
extern SetLightColor_fn SetLightColorPtr;
extern GetLightIntensity_fn GetLightIntensityPtr;
extern SetLightIntensity_fn SetLightIntensityPtr;
extern GetRigidBodyMass_fn GetRigidBodyMassPtr;
extern SetRigidBodyMass_fn SetRigidBodyMassPtr;
extern GetRigidBodyIsKinematic_fn GetRigidBodyIsKinematicPtr;
extern SetRigidBodyIsKinematic_fn SetRigidBodyIsKinematicPtr;
extern GetRigidBodyLinearVelocity_fn GetRigidBodyLinearVelocityPtr;
extern SetRigidBodyLinearVelocity_fn SetRigidBodyLinearVelocityPtr;
extern GetRigidBodyAngularVelocity_fn GetRigidBodyAngularVelocityPtr;
extern SetRigidBodyAngularVelocity_fn SetRigidBodyAngularVelocityPtr;
extern SetBlendShapeWeight_fn SetBlendShapeWeightPtr;
extern GetBlendShapeWeight_fn GetBlendShapeWeightPtr;
extern GetBlendShapeCount_fn GetBlendShapeCountPtr;
extern GetBlendShapeName_fn GetBlendShapeNamePtr;

extern FlushSyncContext_fn   FlushSyncContextPtr;
extern ClearSyncContext_fn   ClearSyncContextPtr;

extern InstallSyncContext_fn InstallSyncContextPtr;
extern EnsureInstalled_fn    EnsureInstalledPtr;

// Navigation interop raw pointer getters (resolved from NavInterop.cpp)
extern "C" void* Get_Nav_FindPath_Ptr();
extern "C" void* Get_Nav_Agent_SetDest_Ptr();
extern "C" void* Get_Nav_Agent_Stop_Ptr();
extern "C" void* Get_Nav_Agent_Warp_Ptr();
extern "C" void* Get_Nav_Agent_Remaining_Ptr();
extern "C" void* Get_Nav_SetOnPathComplete_Ptr();

// IK interop raw pointer getters (resolved from IKInterop.cpp)
extern "C" void* Get_IK_SetWeight_Ptr();
extern "C" void* Get_IK_SetTarget_Ptr();
extern "C" void* Get_IK_SetPole_Ptr();
extern "C" void* Get_IK_SetChain_Ptr();
extern "C" void* Get_IK_GetErrorMeters_Ptr();