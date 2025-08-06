#pragma once
#include <string>
#include "ecs/Entity.h"
#include <vector>
#include <filesystem>

bool LoadHostFxr();
bool LoadDotnetRuntime(const std::wstring& assemblyPath, const std::wstring& typeName, const std::wstring& methodName);


// Script interop function pointer types
using Script_Create_fn = void* (*)(const char* className);
using Script_OnCreate_fn = void (*)(void* handle, int entityID);
using Script_OnUpdate_fn = void (*)(void* handle, float dt);
using ReloadScripts_fn = int (*)(const wchar_t*);

// These are resolved at runtime
extern Script_Create_fn g_Script_Create;
extern Script_OnCreate_fn g_Script_OnCreate;
extern Script_OnUpdate_fn g_Script_OnUpdate;
extern ReloadScripts_fn g_ReloadScripts;
// Creates and initializes the managed script instance for an entity
void CallOnCreate(void* instance, int entityID);
void CallOnUpdate(void* instance, float dt);
void ReloadScripts();
void SetupEntityInterop(std::filesystem::path fullPath);

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
using GetEntityScale_fn        = void(*)(int entityID, float* outX, float* outY, float* outZ);
using SetEntityScale_fn        = void(*)(int entityID, float x, float y, float z);
// Physics
using SetLinearVelocity_fn     = void(*)(int entityID, float x, float y, float z);
using SetAngularVelocity_fn    = void(*)(int entityID, float x, float y, float z);
// Lighting
using SetLightColor_fn         = void(*)(int entityID, float r, float g, float b);
using SetLightIntensity_fn     = void(*)(int entityID, float intensity);
// BlendShapes
using SetBlendShapeWeight_fn   = void(*)(int entityID, const char* shapeName, float weight);
// SyncContext flush
using FlushSyncContext_fn      = void(*)();
using RegisterScriptCallbackFn = void(*)(const char*);


// Pointers to assign in Claymore main.cpp or initialization
extern GetEntityPosition_fn   GetEntityPositionPtr;
extern SetEntityPosition_fn   SetEntityPositionPtr;
extern FindEntityByName_fn    FindEntityByNamePtr;
extern CreateEntity_fn        CreateEntityPtr;
extern DestroyEntity_fn       DestroyEntityPtr;
extern GetEntityRotation_fn   GetEntityRotationPtr;
extern SetEntityRotation_fn   SetEntityRotationPtr;
extern GetEntityScale_fn      GetEntityScalePtr;
extern SetEntityScale_fn      SetEntityScalePtr;
extern SetLinearVelocity_fn   SetLinearVelocityPtr;
extern SetAngularVelocity_fn  SetAngularVelocityPtr;
extern SetLightColor_fn       SetLightColorPtr;
extern SetLightIntensity_fn   SetLightIntensityPtr;
extern SetBlendShapeWeight_fn SetBlendShapeWeightPtr;
extern FlushSyncContext_fn   FlushSyncContextPtr;
