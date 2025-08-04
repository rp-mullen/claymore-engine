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

// These are resolved at runtime
extern Script_Create_fn g_Script_Create;
extern Script_OnCreate_fn g_Script_OnCreate;
extern Script_OnUpdate_fn g_Script_OnUpdate;

void* CreateScriptInstance(const std::string& className);
void CallOnCreate(void* instance, Entity entity);
void CallOnUpdate(void* instance, float dt);
void ReloadScripts();
void SetupEntityInterop(std::filesystem::path fullPath);


// Function pointer types
using GetEntityPosition_fn = void(*)(int entityID, float* outX, float* outY, float* outZ);
using SetEntityPosition_fn = void(*)(int entityID, float x, float y, float z);
using FindEntityByName_fn = int (*)(const char* name);

// Pointers to assign in Claymore main.cpp or initialization
extern GetEntityPosition_fn GetEntityPositionPtr;
extern SetEntityPosition_fn SetEntityPositionPtr;
extern FindEntityByName_fn  FindEntityByNamePtr;