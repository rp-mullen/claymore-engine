#include "DotnetHost.h"
extern "C" {
#include "nethost.h"
   }
#include "coreclr_delegates.h"
#include "hostfxr.h"

#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <fstream>
#include "scripting/ScriptSystem.h"
#include "pipeline/AssetPipeline.h"

// --------------------------------------------------------------------------------------
extern "C" void GetEntityPosition(int entityID, float* outX, float* outY, float* outZ);
extern "C" void SetEntityPosition(int entityID, float x, float y, float z);
extern "C" int FindEntityByName(const char* name);
// --------------------------------------------------------------------------------------


#define STR(s) L##s
using RegisterAllScriptsFn = void(CORECLR_DELEGATE_CALLTYPE*)(void* fnPtr);

// Stored once after load
static RegisterAllScriptsFn g_RegisterAllScripts = nullptr;
// ----------------------------------------
// HostFXR function pointers
// ----------------------------------------
static hostfxr_initialize_for_runtime_config_fn init_fptr = nullptr;
static hostfxr_get_runtime_delegate_fn get_delegate_fptr = nullptr;
static hostfxr_close_fn close_fptr = nullptr;
static load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer = nullptr;

// ----------------------------------------
// Script interop function pointers
// ----------------------------------------
Script_Create_fn g_Script_Create = nullptr;
Script_OnCreate_fn g_Script_OnCreate = nullptr;
Script_OnUpdate_fn g_Script_OnUpdate = nullptr;

// ----------------------------------------
// Managed script registration
// ----------------------------------------
static std::vector<std::string> g_RegisteredScriptNames;

using RegisterScriptCallbackFn = void(CORECLR_DELEGATE_CALLTYPE*)(const char* className);

// Global function pointer set from managed side
static RegisterScriptCallbackFn g_RegisterScriptCallback = nullptr;

// Called by native to let C# perform registration
extern "C" __declspec(dllexport) void CORECLR_DELEGATE_CALLTYPE RegisterScriptType(const char* className)
   {
   g_RegisteredScriptNames.emplace_back(className);
   std::cout << "[Interop] Registered script type: " << className << std::endl;

   ScriptSystem::Instance().RegisterManaged(className);
   }



extern "C" void CORECLR_DELEGATE_CALLTYPE RegisterScriptCallback(const char* className)
   {
   RegisterScriptType(className);
   }

using RegisterScriptDelegateFn = void(CORECLR_DELEGATE_CALLTYPE*)(const char*);

static RegisterScriptDelegateFn s_registerScriptDelegate = nullptr;

// ----------------------------------------
// Load HostFXR + CoreCLR
// ----------------------------------------
bool LoadHostFxr()
   {
   HMODULE nethost = LoadLibraryW(L"nethost.dll");
   if (!nethost)
      {
      std::wcerr << L"[Interop] Failed to load nethost.dll\n";
      return false;
      }

   using get_hostfxr_path_fn = int(__cdecl*)(char_t*, size_t*, const void*);
   auto get_hostfxr_path = (get_hostfxr_path_fn)GetProcAddress(nethost, "get_hostfxr_path");
   if (!get_hostfxr_path)
      {
      std::cerr << "[Interop] Failed to resolve get_hostfxr_path\n";
      return false;
      }

   wchar_t buffer[MAX_PATH];
   size_t size = sizeof(buffer) / sizeof(wchar_t);
   if (get_hostfxr_path(buffer, &size, nullptr) != 0)
      {
      std::wcerr << L"[Interop] get_hostfxr_path failed\n";
      return false;
      }

   HMODULE hostfxr = LoadLibraryW(buffer);
   if (!hostfxr)
      {
      std::wcerr << L"[Interop] Failed to load hostfxr.dll from: " << buffer << std::endl;
      return false;
      }

   init_fptr = (hostfxr_initialize_for_runtime_config_fn)GetProcAddress(hostfxr, "hostfxr_initialize_for_runtime_config");
   get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)GetProcAddress(hostfxr, "hostfxr_get_runtime_delegate");
   close_fptr = (hostfxr_close_fn)GetProcAddress(hostfxr, "hostfxr_close");

   return (init_fptr && get_delegate_fptr && close_fptr);
   }

// ----------------------------------------
// Load .NET runtime and resolve entry point
// ----------------------------------------
bool LoadDotnetRuntime(const std::wstring& assemblyPath, const std::wstring& typeName, const std::wstring& methodName)
   {
   wprintf(L"[Interop] Starting .NET runtime load...\n");

   if (!LoadHostFxr())
      {
      wprintf(L"[Interop] LoadHostFxr() failed.\n");
      return false;
      }

   hostfxr_handle handle = nullptr;
   int rc = init_fptr(L"ClaymoreEngine.runtimeconfig.json", nullptr, &handle);
   if (rc != 0 || handle == nullptr)
      {
      wprintf(L"[Interop] init_fptr failed. HRESULT: 0x%08X\n", rc);
      return false;
      }

   rc = get_delegate_fptr(handle, hdt_load_assembly_and_get_function_pointer, (void**)&load_assembly_and_get_function_pointer);
   close_fptr(handle);
   if (rc != 0 || !load_assembly_and_get_function_pointer)
      {
      wprintf(L"[Interop] get_delegate_fptr failed. HRESULT: 0x%08X\n", rc);
      return false;
      }

   std::filesystem::path fullPath = std::filesystem::absolute(assemblyPath);

   // Force load a known method to warm up the runtime
   void* dummy = nullptr;
   load_assembly_and_get_function_pointer(
      fullPath.c_str(),
      L"System.Object, System.Private.CoreLib",
      L"ToString",
      L"",
      nullptr,
      &dummy
   );

   // Load the managed engine entry point
   using component_entry_point_fn = int (CORECLR_DELEGATE_CALLTYPE*)(void*, int);
   component_entry_point_fn entryPoint = nullptr;

   rc = load_assembly_and_get_function_pointer(
      fullPath.c_str(),
      typeName.c_str(),
      methodName.c_str(),
      L"ClaymoreEngine.EntryPointDelegate, ClaymoreEngine",
      nullptr,
      (void**)&entryPoint
   );

   if (rc != 0 || !entryPoint)
      {
      wprintf(L"[Interop] Failed to get managed entry point. HRESULT: 0x%08X\n", rc);
      return false;
      }

   wprintf(L"[Interop] Managed entry point resolved. Invoking...\n");
   entryPoint(nullptr, 0);
   wprintf(L"[Interop] Entry point completed.\n");

   wchar_t exePath[MAX_PATH];
   GetModuleFileNameW(NULL, exePath, MAX_PATH);
   std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
   std::filesystem::path gameScriptsDllPath = exeDir / "GameScripts.dll";
   if (!std::filesystem::exists(gameScriptsDllPath)) {
       std::wcerr << L"[Interop] Missing GameScripts.dll. Attempting to import scripts...\n";
       AssetPipeline::Instance().CheckAndCompileScriptsAtStartup();
   }

   // Load C# interop exports
   rc = load_assembly_and_get_function_pointer(
      fullPath.c_str(),
      L"ClaymoreEngine.InteropExports, ClaymoreEngine",
      L"Script_Create",
      L"ClaymoreEngine.Script_CreateDelegate, ClaymoreEngine",
      nullptr,
      (void**)&g_Script_Create
   );

   rc |= load_assembly_and_get_function_pointer(
      fullPath.c_str(),
      L"ClaymoreEngine.InteropExports, ClaymoreEngine",
      L"Script_OnCreate",
      L"ClaymoreEngine.Script_OnCreateDelegate, ClaymoreEngine",
      nullptr,
      (void**)&g_Script_OnCreate
   );

   rc |= load_assembly_and_get_function_pointer(
      fullPath.c_str(),
      L"ClaymoreEngine.InteropExports, ClaymoreEngine",
      L"Script_OnUpdate",
      L"ClaymoreEngine.Script_OnUpdateDelegate, ClaymoreEngine",
      nullptr,
      (void**)&g_Script_OnUpdate
   );

   if (!g_Script_Create || !g_Script_OnCreate || !g_Script_OnUpdate)
      {
      std::cerr << "[Interop] Failed to resolve one or more script interop functions.\n";
      return false;
      }

   // Resolve RegisterAllScripts on startup
   if (!g_RegisterAllScripts)
      {
      void* fn = nullptr;
      int rc = load_assembly_and_get_function_pointer(
         fullPath.c_str(),
         L"ClaymoreEngine.InteropExports, ClaymoreEngine",
         L"RegisterAllScripts",
         L"ClaymoreEngine.RegisterAllScriptsDelegate, ClaymoreEngine",
         nullptr,
         &fn
      );

      if (rc == 0 && fn)
         g_RegisterAllScripts = reinterpret_cast<RegisterAllScriptsFn>(fn);
      }

   if (g_RegisterAllScripts)
      g_RegisterAllScripts((void*)&RegisterScriptType);

   SetupEntityInterop(fullPath);

   return true;
   }

// ----------------------------------------
// Reload C# Scripts
// ----------------------------------------
   

void ReloadScripts()
   {
   std::wstring scriptsDllW = L"C:\\Projects\\claymore\\out\\build\\x64-Debug\\GameScripts.dll";

   using ReloadScriptsFn = int (CORECLR_DELEGATE_CALLTYPE*)(const wchar_t*);
   static ReloadScriptsFn s_reloadScripts = nullptr;

   if (!s_reloadScripts)
      {
      void* fn = nullptr;
      int rc = load_assembly_and_get_function_pointer(
         L"C:\\Projects\\claymore\\out\\build\\x64-Debug\\ClaymoreEngine.dll",
         L"ClaymoreEngine.InteropProcessor, ClaymoreEngine",
         L"ReloadScripts",
         L"ClaymoreEngine.ReloadScriptsDelegate, ClaymoreEngine",
         nullptr,
         &fn
      );

      if (rc != 0 || !fn)
         {
         std::cerr << "[Interop] Failed to resolve ReloadScripts.\n";
         return;
         }

      s_reloadScripts = reinterpret_cast<ReloadScriptsFn>(fn);
      }

   int rc = s_reloadScripts(scriptsDllW.c_str());
   if (rc != 0)
      {
      std::cerr << "[Interop] ReloadScripts returned error.\n";
      return;
      }

   std::cout << "[Interop] Scripts reloaded.\n";

   if (!g_RegisterAllScripts)
      {
      void* fn = nullptr;
      int rc = load_assembly_and_get_function_pointer(
         L"C:\\Projects\\claymore\\out\\build\\x64-Debug\\ClaymoreEngine.dll",
         L"ClaymoreEngine.InteropExports, ClaymoreEngine",
         L"RegisterAllScripts",
         L"ClaymoreEngine.RegisterAllScriptsDelegate, ClaymoreEngine",
         nullptr,
         &fn
      );

      if (rc != 0 || !fn)
         {
         std::cerr << "[Interop] Failed to resolve RegisterAllScripts.\n";
         return;
         }

      g_RegisterAllScripts = reinterpret_cast<RegisterAllScriptsFn>(fn);
      }

   // Pass native-to-managed callback pointer
   g_RegisterAllScripts((void*)&RegisterScriptType);
   }

// ----------------------------------------
// C++ Wrapper Utilities
// ----------------------------------------
void* CreateScriptInstance(const std::string& className)
   {
   return g_Script_Create ? g_Script_Create(className.c_str()) : nullptr;
   }

void CallOnCreate(void* instance, Entity entity)
   {
   if (g_Script_OnCreate)
      g_Script_OnCreate(instance, entity.GetID());
   }

void CallOnUpdate(void* instance, float dt)
   {
   if (g_Script_OnUpdate)
      g_Script_OnUpdate(instance, dt);
   }

void SetupEntityInterop(std::filesystem::path fullPath)
{
    if (!g_RegisterScriptCallback)
    {
        void* initArgs[] =
        {
           (void*)GetEntityPositionPtr,
           (void*)SetEntityPositionPtr,
           (void*)FindEntityByNamePtr
        };

        using EntityInteropInitFn = void(*)(void**, int);

        EntityInteropInitFn initInteropFn = nullptr;

        int rc = load_assembly_and_get_function_pointer(
            fullPath.c_str(),
            L"ClaymoreEngine.EntityInterop, ClaymoreEngine",
            L"InitializeInteropExport",
            L"ClaymoreEngine.EntityInteropInitDelegate, ClaymoreEngine",
            nullptr,
            (void**)&initInteropFn
        );


        if (rc != 0)
        {
            std::wcerr << L"[Interop] Failed to get delegate. HRESULT: 0x" << std::hex << rc << std::endl;
        }

        if (rc == 0 && initInteropFn)
            initInteropFn(initArgs, 3);
    }
}