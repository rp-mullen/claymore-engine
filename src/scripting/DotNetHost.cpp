#include "DotNetHost.h"
extern "C" {
#include "nethost.h"
   }
#include "coreclr_delegates.h"
#include "hostfxr.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include <iostream>
#include "ui/Logger.h"
#include <fstream>
#include "scripting/ScriptSystem.h"
#include "pipeline/AssetPipeline.h"
#include "scripting/InputInterop.h"
#include "scripting/ScriptReflectionInterop.h"
#include "navigation/NavInterop.h" // for Get_Nav_*_Ptr declarations
#include "scripting/ComponentInterop.h"
#include <filesystem>
#include <navigation/NavInterop.h>

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
Script_Invoke_fn g_Script_Invoke = nullptr;
ReloadScripts_fn g_ReloadScripts = nullptr;
Script_Destroy_fn g_Script_Destroy = nullptr;

InstallSyncContext_fn InstallSyncContextPtr = nullptr;
EnsureInstalled_fn    EnsureInstalledPtr = nullptr;


// SyncContext control pointers
FlushSyncContext_fn FlushSyncContextPtr = nullptr;
ClearSyncContext_fn ClearSyncContextPtr = nullptr;

// Struct passed to managed side for script registration callbacks
struct ScriptRegistrationInterop {
    void (*RegisterScriptType)(const char*);
    void (*RegisterScriptProperty)(const char*, const char*, int, void*);
};
static ScriptRegistrationInterop g_ScriptRegInterop = { &NativeRegisterScriptType, &RegisterScriptPropertyNative };



// ----------------------------------------
// Managed script registration
// ----------------------------------------
// Vector defined in UILayer.cpp so the UI and interop use the same list
extern std::vector<std::string> g_RegisteredScriptNames;
 

// Global function pointer set from managed side
// Called by native to let C# perform registration
extern "C" __declspec(dllexport) void NativeRegisterScriptType(const char* className)
{
    if(std::find(g_RegisteredScriptNames.begin(), g_RegisteredScriptNames.end(), className) == g_RegisteredScriptNames.end())
        g_RegisteredScriptNames.emplace_back(className);
    std::cout << "[Interop] Registered script type: " << className << std::endl;

    ScriptSystem::Instance().RegisterManaged(className);
}



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

   // Ensure GameScripts.dll is present and compiled BEFORE we invoke the managed entry point
   wchar_t exePath[MAX_PATH];
   GetModuleFileNameW(NULL, exePath, MAX_PATH);
   std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
   std::filesystem::path gameScriptsDllPath = exeDir / "GameScripts.dll";

   if (!std::filesystem::exists(gameScriptsDllPath) || !AssetPipeline::Instance().AreScriptsCompiled()) {
       std::wcerr << L"[Interop] GameScripts.dll missing or out-of-date. Attempting compilation...\n";
       AssetPipeline::Instance().CheckAndCompileScriptsAtStartup();
   }

   if (!AssetPipeline::Instance().AreScriptsCompiled()) {
       Logger::LogError("[Interop] C# scripts failed to compile. Continuing without user scripts – Play Mode disabled.");
   }

   std::cout << "[C++] Calling ManagedStart on thread: " << GetCurrentThreadId() << "\n";
   wprintf(L"[Interop] Managed entry point resolved. Invoking...\n");

   auto CallEntryPointSafe = [](component_entry_point_fn fn)->bool {
       __try {
           fn(nullptr, 0);
           return true;
       } __except(EXCEPTION_EXECUTE_HANDLER) {
           return false;
       }
   };

   bool entryOk = CallEntryPointSafe(entryPoint);
   if(!entryOk) {
       Logger::LogError("[Interop] Managed entry point threw an exception – GameScripts.dll may be corrupted.");
       AssetPipeline::Instance().SetScriptsCompiled(false);
       wprintf(L"[Interop] Entry point failed – runtime loaded without user scripts.\n");
   } else {
       wprintf(L"[Interop] Entry point completed.\n");
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

   {
      int localRc = load_assembly_and_get_function_pointer(
         fullPath.c_str(),
         L"ClaymoreEngine.InteropExports, ClaymoreEngine",
         L"Script_OnUpdate",
         L"ClaymoreEngine.Script_OnUpdateDelegate, ClaymoreEngine",
         nullptr,
         (void**)&g_Script_OnUpdate);
      if(localRc != 0 || (uintptr_t)g_Script_OnUpdate < 0x1000)
      {
         std::cerr << "[Interop] Failed to resolve Script_OnUpdate (HRESULT=" << std::hex << localRc << ")\n";
         g_Script_OnUpdate = nullptr;
      }
      rc |= localRc;
   }

   // Resolve Script_Invoke for arbitrary method calls
   {
      void* fn = nullptr;
      int localRc = load_assembly_and_get_function_pointer(
         fullPath.c_str(),
         L"ClaymoreEngine.InteropExports, ClaymoreEngine",
         L"Script_Invoke",
         L"ClaymoreEngine.Script_InvokeDelegate, ClaymoreEngine",
         nullptr,
         &fn
      );
      if (localRc == 0 && fn)
      {
         g_Script_Invoke = reinterpret_cast<Script_Invoke_fn>(fn);
      }
      else
      {
         std::cerr << "[Interop] Failed to resolve Script_Invoke (HRESULT=" << std::hex << localRc << ")\n";
      }
   } 

   // Load FlushSyncContext from managed side
   rc |= load_assembly_and_get_function_pointer(
      fullPath.c_str(),
      L"ClaymoreEngine.EngineSyncContext, ClaymoreEngine",
      L"Flush",
      L"ClaymoreEngine.VoidDelegate, ClaymoreEngine",
      nullptr,
      (void**)&FlushSyncContextPtr
   );

   // Load ClearSyncContext from managed side (optional safety)
   {
      void* fn = nullptr;
      int localRc = load_assembly_and_get_function_pointer(
         fullPath.c_str(),
         L"ClaymoreEngine.EngineSyncContext, ClaymoreEngine",
         L"Clear",
         L"ClaymoreEngine.VoidDelegate, ClaymoreEngine",
         nullptr,
         &fn
      );
      if (localRc == 0 && fn)
         ClearSyncContextPtr = reinterpret_cast<ClearSyncContext_fn>(fn);
   }

   // Resolve Script_Destroy so native can free managed GCHandles when scripts are destroyed
   {
      void* fn = nullptr;
      int localRc = load_assembly_and_get_function_pointer(
         fullPath.c_str(),
         L"ClaymoreEngine.InteropExports, ClaymoreEngine",
         L"Script_Destroy",
         L"ClaymoreEngine.Script_DestroyDelegate, ClaymoreEngine",
         nullptr,
         &fn
      );
      if (localRc == 0 && fn)
      {
         g_Script_Destroy = reinterpret_cast<Script_Destroy_fn>(fn);
      }
      else
      {
         std::cerr << "[Interop] Failed to resolve Script_Destroy (HRESULT=" << std::hex << localRc << ")\n";
      }
   }

   {
   void* fn = nullptr;
   int rc = load_assembly_and_get_function_pointer(
      fullPath.c_str(),
      L"ClaymoreEngine.EngineSyncContext, ClaymoreEngine",
      L"InstallFromNative",
      L"ClaymoreEngine.VoidDelegate, ClaymoreEngine", // same void() shape
      nullptr,
      &fn
   );
   if (rc == 0 && fn)
      InstallSyncContextPtr = reinterpret_cast<InstallSyncContext_fn>(fn);
   else
      std::cerr << "[Interop] Failed to resolve EngineSyncContextInstaller.Install\n";
   }

   {
   void* fn = nullptr;
   int rc = load_assembly_and_get_function_pointer(
      fullPath.c_str(),
      L"ClaymoreEngine.EngineSyncContext, ClaymoreEngine",
      L"EnsureInstalledHereFromNative",
      L"ClaymoreEngine.VoidDelegate, ClaymoreEngine",
      nullptr,
      &fn
   );
   if (rc == 0 && fn)
      EnsureInstalledPtr = reinterpret_cast<EnsureInstalled_fn>(fn);
   else
      std::cerr << "[Interop] Failed to resolve EngineSyncContextInstaller.EnsureInstalledHere\n";
   }

   if (!g_Script_Create || !g_Script_OnCreate || !g_Script_OnUpdate)
   {
      std::cerr << "[Interop] One or more script interop function pointers are null. "
                << "Create=" << g_Script_Create << ", OnCreate=" << g_Script_OnCreate
                << ", OnUpdate=" << g_Script_OnUpdate << "\n";
      return false;
   }

   if (!FlushSyncContextPtr) {
	   std::cerr << "[Interop] Failed to resolve FlushSyncContext function.\n";
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
   {
       // Pass struct of native callbacks so managed side can invoke them
       g_RegisterAllScripts(reinterpret_cast<void*>(&g_ScriptRegInterop));
   }

   SetupEntityInterop(fullPath);
   SetupInputInterop(fullPath);
   SetupReflectionInterop(fullPath);

   // Navigation interop bootstrap
   {
       void* navArgs[6];
       navArgs[0] = (void*)Get_Nav_FindPath_Ptr();
       navArgs[1] = (void*)Get_Nav_Agent_SetDest_Ptr();
       navArgs[2] = (void*)Get_Nav_Agent_Stop_Ptr();
       navArgs[3] = (void*)Get_Nav_Agent_Warp_Ptr();
       navArgs[4] = (void*)Get_Nav_Agent_Remaining_Ptr();
       navArgs[5] = (void*)Get_Nav_SetOnPathComplete_Ptr();
       using NavInteropInitFn = void(*)(void**, int);
       NavInteropInitFn initNavFn = nullptr;
       int rcNav = load_assembly_and_get_function_pointer(
           fullPath.c_str(),
           L"ClaymoreEngine.NavigationInterop, ClaymoreEngine",
           L"InitializeInteropExport",
           L"ClaymoreEngine.NavigationInteropInitDelegate, ClaymoreEngine",
           nullptr,
           (void**)&initNavFn
       );
       if (rcNav == 0 && initNavFn) {
           initNavFn(navArgs, 6);
       }
   }

   // IK interop bootstrap
   {
       void* ikArgs[5];
       ikArgs[0] = (void*)Get_IK_SetWeight_Ptr();
       ikArgs[1] = (void*)Get_IK_SetTarget_Ptr();
       ikArgs[2] = (void*)Get_IK_SetPole_Ptr();
       ikArgs[3] = (void*)Get_IK_SetChain_Ptr();
       ikArgs[4] = (void*)Get_IK_GetErrorMeters_Ptr();

       using IKInteropInitFn = void(*)(void**, int);
       IKInteropInitFn initIkFn = nullptr;
       int rcIk = load_assembly_and_get_function_pointer(
           fullPath.c_str(),
           L"ClaymoreEngine.IKInterop, ClaymoreEngine",
           L"InitializeInteropExport",
           L"ClaymoreEngine.IKInteropInitDelegate, ClaymoreEngine",
           nullptr,
           (void**)&initIkFn
       );
       if (rcIk == 0 && initIkFn) {
           initIkFn(ikArgs, 5);
       }
   }

   return true;
   }

// ----------------------------------------
// Reload C# Scripts
// ----------------------------------------
   
    

void ReloadScripts()
   {
   wchar_t exePath[MAX_PATH];
   GetModuleFileNameW(NULL, exePath, MAX_PATH);
   std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
   std::filesystem::path scriptsDLL = exeDir / L"GameScripts.dll";
   std::wstring scriptsDllW = scriptsDLL.wstring();

   std::filesystem::path engineDLL = exeDir / L"ClaymoreEngine.dll";

   if (!g_ReloadScripts)
      {
      void* fn = nullptr;
      int rc = load_assembly_and_get_function_pointer(
         engineDLL.wstring().c_str(),
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

      g_ReloadScripts = reinterpret_cast<ReloadScripts_fn>(fn);
      }

   if (!g_ReloadScripts) {
        std::cerr << "[Interop] ReloadScripts function pointer not set.\n";
        return;
    }

    int rc = g_ReloadScripts(scriptsDllW.c_str());
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
         engineDLL.wstring().c_str(),
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

// Pass struct of callbacks
   g_RegisterAllScripts(reinterpret_cast<void*>(&g_ScriptRegInterop));
   }
// ----------------------------------------
// C++ Wrapper Utilities
// ----------------------------------------
void* CreateScriptInstance(const std::string& className)
   {
   return g_Script_Create ? g_Script_Create(className.c_str()) : nullptr;
   }

void CallOnCreate(void* instance, int entityID)
{
    if(!instance)
        return;
    if (g_Script_OnCreate)
        g_Script_OnCreate(instance, entityID);
}

void CallOnUpdate(void* instance, float dt)
{
    if(!instance)
        return; // Skip if script handle invalid

    if (g_Script_OnUpdate)
        g_Script_OnUpdate(instance, dt);
}

void SetupEntityInterop(std::filesystem::path fullPath)
{
    static bool s_EntityInteropInitialized = false;
    if (s_EntityInteropInitialized)
        return;

    void* initArgs[] =
        {
           (void*)GetEntityPositionPtr,
           (void*)SetEntityPositionPtr,
           (void*)FindEntityByNamePtr,
           (void*)CreateEntityPtr,
           (void*)DestroyEntityPtr,
           (void*)GetEntityByIDPtr,
           (void*)GetEntityRotationPtr,
           (void*)SetEntityRotationPtr,
           (void*)GetEntityRotationQuatPtr,
           (void*)SetEntityRotationQuatPtr,
           (void*)GetEntityScalePtr,
           (void*)SetEntityScalePtr,
           (void*)SetLinearVelocityPtr,
           (void*)SetAngularVelocityPtr,

           // Component Interop
           (void*)HasComponentPtr,
           (void*)AddComponentPtr,
           (void*)RemoveComponentPtr,
           (void*)GetLightTypePtr,
           (void*)SetLightTypePtr,
           (void*)GetLightColorPtr,
           (void*)SetLightColorPtr,
           (void*)GetLightIntensityPtr,
           (void*)SetLightIntensityPtr,
           (void*)GetRigidBodyMassPtr,
           (void*)SetRigidBodyMassPtr,
           (void*)GetRigidBodyIsKinematicPtr,
           (void*)SetRigidBodyIsKinematicPtr,
           (void*)GetRigidBodyLinearVelocityPtr,
           (void*)SetRigidBodyLinearVelocityPtr, 
           (void*)GetRigidBodyAngularVelocityPtr,
           (void*)SetRigidBodyAngularVelocityPtr,
           (void*)SetBlendShapeWeightPtr,
           (void*)GetBlendShapeWeightPtr,
           (void*)GetBlendShapeCountPtr,
            (void*)GetBlendShapeNamePtr,

            // Animator parameter setters (5)
            (void*)&Animator_SetBool,
            (void*)&Animator_SetInt,
            (void*)&Animator_SetFloat,
            (void*)&Animator_SetTrigger,
            (void*)&Animator_ResetTrigger,
            (void*)&Animator_GetBool,
            (void*)&Animator_GetInt,
            (void*)&Animator_GetFloat,
            (void*)&Animator_GetTrigger,

            // UI Buttons (3)
            (void*)&UI_ButtonIsHovered,
            (void*)&UI_ButtonIsPressed,
            (void*)&UI_ButtonWasClicked
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
        {
            initInteropFn(initArgs, sizeof(initArgs) / sizeof(void*));
            s_EntityInteropInitialized = true;
        }
    }

void SetupInputInterop(std::filesystem::path fullPath)
{
    static bool s_InputInteropInitialized = false;
    if(s_InputInteropInitialized)
        return;

    // Ensure reflection setter is ready
    SetupReflectionInterop(fullPath);

    void* initArgs[] = {
        (void*)IsKeyHeldPtr,
        (void*)IsKeyDownPtr,
        (void*)IsMouseDownPtr,
        (void*)GetMouseDeltaPtr,
        (void*)DebugLogPtr,
        (void*)SetMouseModePtr,
        (void*)SetManagedFieldPtr
    };

    using InputInteropInitFn = void(*)(void**, int);
    InputInteropInitFn initInteropFn = nullptr;

    int rc = load_assembly_and_get_function_pointer(
        fullPath.c_str(),
        L"ClaymoreEngine.InputInterop, ClaymoreEngine",
        L"InitializeInteropExport",
        L"ClaymoreEngine.InputInteropInitDelegate, ClaymoreEngine",
        nullptr,
        (void**)&initInteropFn);

    if(rc != 0)
    {
        std::wcerr << L"[Interop] Failed to get InputInterop delegate. HRESULT: 0x" << std::hex << rc << std::endl;
        return;
    }
    if(initInteropFn)
    {
        initInteropFn(initArgs, static_cast<int>(sizeof(initArgs)/sizeof(void*)));
        s_InputInteropInitialized = true;
    }
}

void SetupReflectionInterop(std::filesystem::path fullPath)
{
    if(SetManagedFieldPtr) return;

    void* fn = nullptr;
    int rc = load_assembly_and_get_function_pointer(
        fullPath.c_str(),
        L"ClaymoreEngine.InteropExports, ClaymoreEngine",
        L"SetManagedField",
        L"ClaymoreEngine.InteropExports+SetFieldDelegate, ClaymoreEngine",
        nullptr,
        &fn); 
    if(rc==0 && fn)
        SetManagedFieldPtr = reinterpret_cast<SetManagedField_fn>(fn);
}


