#pragma once

#include <string>

namespace DotnetBridge {
   bool InitializeRuntime(const std::wstring& runtimeConfigPath); // e.g. dotnet.runtimeconfig.json
   bool InvokeManagedEntry(const std::wstring& assemblyPath,
      const std::wstring& typeName,
      const std::wstring& methodName);
   }
