#pragma once
#include <bgfx/bgfx.h>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <functional>

enum class ShaderType
   {
   Vertex,
   Fragment,
   Compute
   };

class ShaderManager
   {
   public:
      static ShaderManager& Instance();

      bgfx::ShaderHandle LoadShader(const std::string& name, ShaderType type);
      bgfx::ProgramHandle LoadProgram(const std::string& vsName, const std::string& fsName);

      // Compile all shaders found in the executable's shaders directory if out-of-date or missing bin.
      void CompileAllShaders();

      bgfx::ShaderHandle CompileAndCache(const std::string& path, ShaderType type);

   private:
      ShaderManager() = default;

      bool CompileShader(const std::string& name, ShaderType type);

      std::atomic<bool> m_Watching{ false };
      std::thread m_WatchThread;

      std::mutex m_ProgramMutex;
      std::unordered_map<std::string, bgfx::ProgramHandle> m_Programs;

      std::unordered_map<std::string, bgfx::ShaderHandle> m_ShaderCache; // name -> handle
      std::mutex m_ShaderMutex;

      std::function<void(const std::string&)> m_ReloadCallback;
   };
