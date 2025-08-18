#include "ShaderManager.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <thread>
#include "io/FileSystem.h"
#include "ShaderBundle.h"

namespace fs = std::filesystem;

ShaderManager& ShaderManager::Instance()
   {
   static ShaderManager instance;
   return instance;
   }

static std::string GetBackendFolder(bgfx::RendererType::Enum renderer)
   {
   switch (renderer)
      {
         case bgfx::RendererType::Direct3D11:
         case bgfx::RendererType::Direct3D12: return "windows";
         case bgfx::RendererType::OpenGL: return "opengl";
         case bgfx::RendererType::Vulkan: return "vulkan";
         case bgfx::RendererType::Metal: return "metal";
         default: return "unknown";
      }
   }

bool ShaderManager::CompileShader(const std::string& name, ShaderType type)
   {
   // In packaged runtime, skip compilation; we rely on precompiled bins in the pak
   if (FileSystem::Instance().IsPakMounted()) {
       return true;
   }
   fs::path exeDir = fs::current_path();  // e.g., build/Release
   fs::path shadersDir = exeDir / "shaders";
   fs::path toolsDir   = exeDir / "tools";

   fs::path shaderSrc = shadersDir / (name + ".sc");
   fs::path outFolder = shadersDir / "compiled" / "windows"; // For now: windows
   fs::path shaderOut = outFolder / (name + ".bin");

   // Create all necessary folders
   fs::create_directories(shaderOut.parent_path());

   if (!fs::exists(shaderSrc)) {
      std::cerr << "[ShaderManager] Source not found: " << shaderSrc << std::endl;
      return false;
      }

   fs::path varyingFile = shadersDir / "varying.def.sc";
    if (fs::exists(shaderOut)) {
        auto binTime = fs::last_write_time(shaderOut);
        if (binTime > fs::last_write_time(shaderSrc) &&
            (!fs::exists(varyingFile) || binTime > fs::last_write_time(varyingFile))) {
            return true; // Up-to-date
        }
    }

   std::string shaderTypeStr = (type == ShaderType::Vertex) ? "vertex" :
      (type == ShaderType::Fragment) ? "fragment" : "compute";

   std::string profile = "s_5_0"; // DX11 default
   std::string cmd = "\"" + (toolsDir / "shaderc.exe").string() + "\""
      + " -f " + shaderSrc.string()
      + " -o " + shaderOut.string()
      + " --type " + shaderTypeStr
      + " --platform windows"
      + " --profile " + profile
      + " --varyingdef " + (shadersDir / "varying.def.sc").string()
      + " -i " + shadersDir.string()
      + " -i " + (shadersDir / "include").string();
    {
        fs::path bgfxInc = exeDir;
        for(int i=0;i<12 && !fs::exists(bgfxInc / "external/bgfx/src/bgfx_shader.sh"); ++i)
            bgfxInc = bgfxInc.parent_path();
        bgfxInc /= "external/bgfx/src";
        cmd += " -i " + bgfxInc.string();
    }

   std::cout << "[ShaderManager] Compiling: " << cmd << std::endl;

   int result = system(cmd.c_str());
   if (result != 0) {
      std::cerr << "[ShaderManager] Failed to compile shader: " << shaderSrc << std::endl;
      return false;
      }
   return true;
   }

static bgfx::ShaderHandle CreateShaderFromFile(const fs::path& path)
   {
   std::vector<uint8_t> data;
   if (!FileSystem::Instance().ReadFile(path.string(), data)) {
       std::cerr << "[ShaderManager] Failed to read shader: \"" << path.string() << "\"" << std::endl;
       return BGFX_INVALID_HANDLE;
   }
   const bgfx::Memory* mem = bgfx::alloc(uint32_t(data.size() + 1));
   if (!data.empty()) memcpy(mem->data, data.data(), data.size());
   mem->data[data.size()] = '\0';

   return bgfx::createShader(mem);
   }

bgfx::ShaderHandle ShaderManager::LoadShader(const std::string& name, ShaderType type)
   {
   fs::path exeDir = fs::current_path();
   fs::path shaderOut;
   if (FileSystem::Instance().IsPakMounted()) {
       // packaged: prefer direct path under compiled folder (VFS handles lookup), but also try plain shaders/<name>.bin
       std::vector<fs::path> candidates = {
           exeDir / "shaders" / "compiled" / "windows" / (name + ".bin"),
           exeDir / "shaders" / (name + ".bin"),
           fs::path("shaders/compiled/windows/") / (name + ".bin"),
           fs::path("shaders/") / (name + ".bin")
       };
       for (auto& c : candidates) {
           std::vector<uint8_t> data;
           if (FileSystem::Instance().ReadFile(c.string(), data) && !data.empty()) {
               std::cout << "[ShaderManager] Using shader bin: " << c.string() << std::endl;
               shaderOut = c; break;
           }
       }
       if (shaderOut.empty()) {
           // Fallback to conventional path; CreateShaderFromFile will still error log if not found
           shaderOut = exeDir / "shaders" / "compiled" / "windows" / (name + ".bin");
       }
   } else {
       shaderOut = exeDir / "shaders" / "compiled" / "windows" / (name + ".bin");
   }
   // In editor mode, ensure compiled; in packaged mode just read from VFS/disk
   if (!FileSystem::Instance().IsPakMounted()) {
       if (!CompileShader(name, type)) {
          return BGFX_INVALID_HANDLE;
       }
   }
   return CreateShaderFromFile(shaderOut);
   }

bgfx::ProgramHandle ShaderManager::LoadProgram(const std::string& vsName, const std::string& fsName)
   {
   bgfx::ShaderHandle vsh = LoadShader(vsName, ShaderType::Vertex);
   bgfx::ShaderHandle fsh = LoadShader(fsName, ShaderType::Fragment);

   if (!bgfx::isValid(vsh)) {
      printf("Vertex Shader Invalid - %s\n", vsName.c_str());
      }
   if (!bgfx::isValid(fsh)) {
      printf("Fragment Shader Invalid - %s\n", fsName.c_str());
      }

   if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
      return BGFX_INVALID_HANDLE;
      }

   bgfx::ProgramHandle program = bgfx::createProgram(vsh, fsh, true);
   {
   std::lock_guard<std::mutex> lock(m_ProgramMutex);
   m_Programs[vsName + "+" + fsName] = program;
   }
   return program;
   }

bgfx::ProgramHandle ShaderManager::LoadProgramFromBundle(const std::string& baseName)
{
    return ShaderBundle::Instance().Load(baseName);
}

void ShaderManager::InvalidateProgram(const std::string& key)
{
    // For legacy programs we keep m_Programs map; for bundles we forward to ShaderBundle
    {
        std::lock_guard<std::mutex> lock(m_ProgramMutex);
        auto it = m_Programs.find(key);
        if (it != m_Programs.end()) {
            if (bgfx::isValid(it->second)) bgfx::destroy(it->second);
            m_Programs.erase(it);
        }
    }
    ShaderBundle::Instance().Invalidate(key);
}

// ------------------- New: CompileAllShaders -------------------
void ShaderManager::CompileAllShaders()
{
    if (FileSystem::Instance().IsPakMounted()) return;
    fs::path exeDir = fs::current_path();
    fs::path shadersDir = exeDir / "shaders";

    // Mirror source shaders into runtime dir if running from build output
    {
        fs::path src = exeDir;
        for (int i=0; i<5 && !fs::exists(src / "shaders"); ++i)
            src = src.parent_path();
        src = src / "shaders";
        if (fs::exists(src) && src != shadersDir) {
            for (auto& entry : fs::recursive_directory_iterator(src)) {
                if (!entry.is_regular_file()) continue;
                fs::path rel = fs::relative(entry.path(), src);
                fs::path dst = shadersDir / rel;
                fs::create_directories(dst.parent_path());
                try { fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing); }
                catch(...){}
            }
        }
    }

    if (!fs::exists(shadersDir))
        return;

    // Ensure varying.def.sc has no UTF-8 BOM
    {
        fs::path varying = shadersDir / "varying.def.sc";
        if (fs::exists(varying)) {
            std::ifstream in(varying, std::ios::binary);
            std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (contents.size() >= 3 && (uint8_t)contents[0] == 0xEF && (uint8_t)contents[1] == 0xBB && (uint8_t)contents[2] == 0xBF) {
                std::cout << "[ShaderManager] Stripping BOM from varying.def.sc\n";
                contents.erase(0, 3);
                std::ofstream out(varying, std::ios::binary | std::ios::trunc);
                out.write(contents.data(), contents.size());
            }
        }
    }

    for (auto& entry : fs::recursive_directory_iterator(shadersDir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".sc") continue;
        if (entry.path().filename() == "varying.def.sc") continue;

        std::string stem = entry.path().stem().string();

        ShaderType type = ShaderType::Fragment; // default
        if (stem.rfind("vs_", 0) == 0) {
            type = ShaderType::Vertex;
        }
        else if (stem.rfind("fs_", 0) == 0 || stem.rfind("ps_", 0) == 0) {
            type = ShaderType::Fragment;
        }
        else if (stem.rfind("cs_", 0) == 0) {
            type = ShaderType::Compute;
        }

        CompileShader(stem, type);
    }
}

bgfx::ShaderHandle ShaderManager::CompileAndCache(const std::string& path, ShaderType type) {
    // existing implementation unchanged ... (placed here for brevity)
    std::lock_guard<std::mutex> lock(m_ShaderMutex);
    std::string shaderName = fs::path(path).stem().string();
    auto it = m_ShaderCache.find(shaderName);
    if (it != m_ShaderCache.end() && bgfx::isValid(it->second))
        return it->second;

    fs::path exeDir = fs::current_path();
    fs::path shadersDir = exeDir / "shaders";
    fs::path toolsDir   = exeDir / "tools";
    fs::path compiledDir= shadersDir / "compiled" / "windows";
    fs::path shaderOut  = compiledDir / (shaderName + ".bin");
    fs::create_directories(shaderOut.parent_path());

    bool needsCompile = true;
    if (fs::exists(shaderOut) && fs::exists(path)) {
        needsCompile = fs::last_write_time(shaderOut) < fs::last_write_time(path);
    }

    if (needsCompile) {
        std::string shaderTypeStr = (type == ShaderType::Vertex) ? "vertex" :
                                    (type == ShaderType::Fragment) ? "fragment" : "compute";
        std::string profile = "s_5_0";
        std::string cmd = "\"" + (toolsDir / "shaderc.exe").string() + "\"";
        cmd += " -f " + path;
        cmd += " -o " + shaderOut.string();
        cmd += " --type " + shaderTypeStr;
        cmd += " --platform windows";
        cmd += " --profile " + profile;
        cmd += " --varyingdef " + (shadersDir / "varying.def.sc").string();
        cmd += " -i " + shadersDir.string();
        cmd += " -i " + (shadersDir / "include").string();
    // Add bgfx built-in shader include path
    fs::path bgfxInc = exeDir;
    for(int i=0;i<12 && !fs::exists(bgfxInc / "external/bgfx/src/bgfx_shader.sh"); ++i)
        bgfxInc = bgfxInc.parent_path();
    bgfxInc /= "external/bgfx/src";
    cmd += " -i " + bgfxInc.string();
        std::cout << "[ShaderManager] Compiling shader: " << path << std::endl;
        if (system(cmd.c_str()) != 0) {
            std::cerr << "[ShaderManager] Failed to compile: " << path << std::endl;
            return BGFX_INVALID_HANDLE;
        }
    }

    bgfx::ShaderHandle handle = CreateShaderFromFile(shaderOut);
    if (bgfx::isValid(handle)) {
        m_ShaderCache[shaderName] = handle;
    }
    return handle;
}
