#include "DotnetBridge.h"
#include "scripting/DotNetHost.h" // calls LoadHostFxr etc.

static bool g_RuntimeInitialized = false;

bool DotnetBridge::InitializeRuntime(const std::wstring& configPath) {
   g_RuntimeInitialized = LoadHostFxr();
   return g_RuntimeInitialized;
   }

bool DotnetBridge::InvokeManagedEntry(const std::wstring& assemblyPath,
   const std::wstring& typeName,
   const std::wstring& methodName) {
   if (!g_RuntimeInitialized)
      return false;

   return LoadDotnetRuntime(assemblyPath, typeName, methodName);
   }
