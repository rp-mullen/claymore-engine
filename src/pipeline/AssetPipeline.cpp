#include "AssetPipeline.h"
#include "AssetRegistry.h"
#include "AssetMetadata.h"
#include "AssetLibrary.h"
#include "rendering/ModelLoader.h"
#include "rendering/TextureLoader.h"
#include "rendering/ShaderManager.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <unordered_set>
#include <openssl/md5.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stb_image.h>
#include "scripting/DotNetHost.h"
#include <editor/Project.h>



namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------------------------------------
// SCAN PROJECT (background safe)
// ---------------------------------------
void AssetPipeline::ScanProject(const std::string& rootPath) {
    for (auto& entry : fs::recursive_directory_iterator(rootPath)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        if (!IsSupportedAsset(ext)) continue;

        std::string filePath = entry.path().string();
        std::string hash = ComputeHash(filePath);

        const auto* meta = GetMetadata(filePath);
        if (!meta || meta->hash != hash) {
            EnqueueAssetImport(filePath);
        }
    }
}

// ---------------------------------------
// QUEUE FOR CPU IMPORT
// ---------------------------------------
void AssetPipeline::EnqueueAssetImport(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_ImportQueue.push(path);
}

// ---------------------------------------
// PROCESS IMPORTS + GPU TASKS
// ---------------------------------------
void AssetPipeline::ProcessMainThreadTasks() {
    // 1. Import queue
    std::queue<std::string> localQueue;
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        std::swap(localQueue, m_ImportQueue);
    }

    while (!localQueue.empty()) {
        ImportAsset(localQueue.front());
        localQueue.pop();
    }

    // 2. Execute scheduled lambdas (e.g., GPU safe)
    std::queue<std::function<void()>> localTaskQueue;
    {
        std::lock_guard<std::mutex> lock(m_MainThreadQueueMutex);
        std::swap(localTaskQueue, m_MainThreadTasks);
    }

    while (!localTaskQueue.empty()) {
        localTaskQueue.front()();
        localTaskQueue.pop();
    }

    // 3. Process GPU upload jobs
    ProcessGPUUploads();
}

void AssetPipeline::ProcessGPUUploads() {
    std::queue<PendingGPUUpload> localGPUQueue;
    {
        std::lock_guard<std::mutex> lock(m_GPUQueueMutex);
        std::swap(localGPUQueue, m_GPUUploadQueue);
    }

    while (!localGPUQueue.empty()) {
        localGPUQueue.front().Upload();
        localGPUQueue.pop();
    }
}

void AssetPipeline::EnqueueMainThreadTask(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(m_MainThreadQueueMutex);
    m_MainThreadTasks.push(std::move(task));
}

void AssetPipeline::EnqueueGPUUpload(PendingGPUUpload&& task) {
    std::lock_guard<std::mutex> lock(m_GPUQueueMutex);
    m_GPUUploadQueue.push(std::move(task));
}

// ---------------------------------------
// IMPORT ASSET
// ---------------------------------------
void AssetPipeline::ImportAsset(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::string metaPath = path + ".meta";

    std::string hash = ComputeFileHash(path);

    AssetMetadata meta;
    bool hasMeta = false;

    if (fs::exists(metaPath)) {
        std::ifstream in(metaPath);
        if (in) {
            json j;
            in >> j;
            meta = j.get<AssetMetadata>();
            hasMeta = true;
        }
    }

    if (hasMeta && meta.hash == hash) return;

    // Dispatch
    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb") {
        ImportModel(path);
        meta.type = "model";
    }
    else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
        ImportTextureCPU(path); // Optimized async texture
        meta.type = "texture";
    }
    else if (ext == ".sc" || ext == ".glsl" || ext == ".shader") {
        ImportShader(path);
        meta.type = "shader";
    }
    else if (ext == ".cs") {
       ImportScript(path);
       meta.type = "script";
       }
    else {
        return;
    }

    meta.sourcePath = path;
    meta.processedPath = "cache/" + fs::path(path).filename().string();
    meta.hash = hash;
    meta.lastImported = GetCurrentTimestamp();
    
            // Generate GUID and asset reference if not already present
        if (meta.guid.high == 0 && meta.guid.low == 0) {
            meta.guid = ClaymoreGUID::Generate();
            meta.reference = AssetReference(meta.guid, 0, static_cast<int32_t>(AssetType::Mesh));
        }

    json j = meta;
    std::ofstream out(metaPath);
    out << j.dump(4);

    AssetRegistry::Instance().SetMetadata(path, meta);
    
    // Register asset in AssetLibrary
    AssetLibrary::Instance().RegisterAsset(meta.reference, 
        static_cast<AssetType>(meta.reference.type), 
        path, 
        fs::path(path).filename().string());

    std::cout << "[AssetPipeline] Imported: " << path << " (GUID: " << meta.guid.ToString() << ")" << std::endl;
}



void AssetPipeline::ImportScript(const std::string& path)
   {
   std::wstring compilerExe = (std::filesystem::current_path() / "tools" / "ScriptCompiler.exe").wstring();

   // Get executable directory
   wchar_t exePath[MAX_PATH];
   GetModuleFileNameW(NULL, exePath, MAX_PATH);
   std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

   // Build DLL paths relative to the executable
   std::filesystem::path gameScriptsDll = exeDir / "GameScripts.dll";
   std::filesystem::path engineDll = exeDir / "ClaymoreEngine.dll";

   // Use Project path
   std::wstring projectPath = std::filesystem::path(Project::GetProjectDirectory()).wstring();

   // Quote all paths and build the args string
   std::wstring args = L"\"" + projectPath + L"\" "
       L"\"" + gameScriptsDll.wstring() + L"\" "
       L"\"" + engineDll.wstring() + L"\"";


   std::wstring commandLine = L"\"" + compilerExe + L"\" " + args;

   STARTUPINFOW si = { sizeof(si) };
   PROCESS_INFORMATION pi;

   BOOL success = CreateProcessW(
      NULL,
      &commandLine[0],  // mutable buffer
      NULL, NULL,
      FALSE,
      0, NULL, NULL,
      &si, &pi
   );

   if (success)
      {
      WaitForSingleObject(pi.hProcess, INFINITE);
      DWORD exitCode;
      GetExitCodeProcess(pi.hProcess, &exitCode);
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);

      if (exitCode == 0) {
         std::cout << "[Roslyn] Successfully compiled C# scripts.\n";
         ReloadScripts(); // Reload scripts after successful compile
         }
      else
         std::cerr << "[Roslyn] Compilation failed.\n";
      }
   else
      {
      std::cerr << "[Roslyn] Failed to launch ScriptCompiler.exe\n";
      }
   }



// ---------------------------------------
// MODEL IMPORT (GPU-safe queued)
// ---------------------------------------
void AssetPipeline::ImportModel(const std::string& path) {
    EnqueueMainThreadTask([path]() {
        auto mesh = ModelLoader::LoadModel(path);
        std::cout << "[AssetPipeline] Model uploaded to GPU: " << path << std::endl;
        });
}

// ---------------------------------------
// TEXTURE IMPORT (CPU -> GPU queue)
// ---------------------------------------
void AssetPipeline::ImportTextureCPU(const std::string& path) {
    int width, height, channels;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        std::cerr << "[AssetPipeline] Failed to load texture: " << path << std::endl;
        return;
    }

    std::vector<uint8_t> pixelData(pixels, pixels + width * height * 4);
    stbi_image_free(pixels);

    PendingGPUUpload task;
    task.type = PendingGPUUpload::Type::Texture;
    task.sourcePath = path;
    task.width = width;
    task.height = height;
    task.pixelData = std::move(pixelData);
    task.Upload = [task]() {
        const bgfx::Memory* mem = bgfx::copy(task.pixelData.data(), task.pixelData.size());
        bgfx::createTexture2D((uint16_t)task.width, (uint16_t)task.height,
            false, 1, task.format, 0, mem);
        std::cout << "[AssetPipeline] Uploaded texture: " << task.sourcePath << std::endl;
        };

    EnqueueGPUUpload(std::move(task));
}

// ---------------------------------------
// SHADER IMPORT (CPU compile -> GPU upload)
// ---------------------------------------
void AssetPipeline::ImportShader(const std::string& path) {
    ShaderType type = ShaderType::Fragment;
    if (path.find("vs_") != std::string::npos) type = ShaderType::Vertex;
    else if (path.find("fs_") != std::string::npos) type = ShaderType::Fragment;

    EnqueueMainThreadTask([path, type]() {
        ShaderManager::Instance().CompileAndCache(path, type);
        std::cout << "[AssetPipeline] Shader compiled and loaded: " << path << std::endl;
        });
}

// ---------------------------------------
// HASH UTILITIES
// ---------------------------------------
std::string AssetPipeline::ComputeFileHash(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream contents;
    contents << file.rdbuf();
    std::string data = contents.str();

    unsigned char result[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), result);

    std::ostringstream hex;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
        hex << std::hex << std::setw(2) << std::setfill('0') << (int)result[i];
    return hex.str();
}

std::string AssetPipeline::ComputeHash(const std::string& path) const {
    auto time = fs::last_write_time(path).time_since_epoch().count();
    return std::to_string(time);
}

// ---------------------------------------
// UTILITIES
// ---------------------------------------
bool AssetPipeline::IsSupportedAsset(const std::string& ext) const {
    static const std::unordered_set<std::string> supported = {
        ".fbx",      // Models
        ".png", ".jpg", ".jpeg", ".tga", // Textures
        ".sc", ".shader", ".glsl", // Shaders
        ".cs"
    };
    return supported.find(ext) != supported.end();
}

std::string AssetPipeline::DetermineType(const std::string& ext) {
    if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") return "model";
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") return "texture";
    if (ext == ".sc" || ext == ".shader" || ext == ".glsl") return "shader";
    if (ext == ".cs") return "script";

    return "unknown";
}

std::string AssetPipeline::GetCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void AssetPipeline::CheckAndCompileScriptsAtStartup()
{
    const std::string scriptsDllPath = "out/build/x64-Debug/GameScripts.dll";
    if (std::filesystem::exists(scriptsDllPath))
    {
        std::cout << "[Startup] GameScripts.dll exists. Skipping script bootstrap.\n";
        return;
    }

    std::cout << "[Startup] GameScripts.dll missing. Scanning for scripts...\n";

    const std::string projectRoot = Project::GetProjectDirectory().string(); // or hardcoded path
    bool foundScript = false;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(projectRoot))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".cs")
        {
            foundScript = true;
            std::cout << "[Startup] Found script: " << entry.path() << "\n";
            ImportScript(entry.path().string());
        }
    }

    if (!foundScript)
    {
        std::cerr << "[Startup] No .cs scripts found in project. Cannot build GameScripts.dll.\n";
    }
}
