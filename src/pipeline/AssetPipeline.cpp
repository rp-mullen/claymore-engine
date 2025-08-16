#include "AssetPipeline.h"
#include "AssetRegistry.h"
#include "AssetMetadata.h"
#include "AssetLibrary.h"
#include "rendering/ModelLoader.h"
#include "rendering/TextureLoader.h"
#include "rendering/ShaderManager.h"
#include "animation/AnimationImporter.h"
#include "animation/AnimationSerializer.h"
#include "ModelImportCache.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
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
#include "ui/Logger.h"
#include <stb_image.h>
#include "scripting/DotNetHost.h"
#include <editor/Project.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <animation/AvatarSerializer.h>

#include "jobs/Jobs.h"
#include "jobs/JobSystem.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------------------------------------
// SCAN PROJECT (background safe)
// ---------------------------------------
void AssetPipeline::ScanProject(const std::string& rootPath) {
    m_LastScanList.clear();
    for (auto& entry : fs::recursive_directory_iterator(rootPath)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        if (!IsSupportedAsset(ext)) continue;

        std::string filePath = entry.path().string();
        m_LastScanList.push_back(filePath);
        std::string hash = ComputeHash(filePath);

        const auto* meta = GetMetadata(filePath);
        if (!meta || meta->hash != hash) {
            EnqueueAssetImport(filePath);
        }
    }
    std::cout << "[AssetPipeline] Scan complete. Assets found: " << m_LastScanList.size() << std::endl;
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

    // 2b. (removed) model-specific queue no longer used; callbacks go through m_MainThreadTasks

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

// Block until current import queue and tasks are processed (called after menu-triggered reimport)
void AssetPipeline::ProcessAllBlocking() {
    // Pump until queues are empty for this frame budget
    int safety = 10000;
    while (safety-- > 0) {
        size_t q1, q2, q3;
        {
            std::lock_guard<std::mutex> l1(m_QueueMutex);
            q1 = m_ImportQueue.size();
        }
        {
            std::lock_guard<std::mutex> l2(m_MainThreadQueueMutex);
            q2 = m_MainThreadTasks.size();
        }
        {
            std::lock_guard<std::mutex> l3(m_GPUQueueMutex);
            q3 = m_GPUUploadQueue.size();
        }
        if (q1 == 0 && q2 == 0 && q3 == 0) break;
        ProcessMainThreadTasks();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
         SetScriptsCompiled(true);
         ReloadScripts(); // Reload scripts after successful compile
         }
      else {
         std::cerr << "[Roslyn] Compilation failed.\n";
         Logger::LogError("[Roslyn] Script compilation failed. Check errors above.");
         SetScriptsCompiled(false);
         }
      }
   else {
      std::cerr << "[Roslyn] Failed to launch ScriptCompiler.exe\n";
      Logger::LogError("[Roslyn] Failed to launch ScriptCompiler.exe");
      SetScriptsCompiled(false);
      }

   }



// ---------------------------------------
// MODEL IMPORT (GPU-safe queued)
// ---------------------------------------
void AssetPipeline::ImportModel(const std::string& path) {
    EnqueueMainThreadTask([path]() {
        auto model = ModelLoader::LoadModel(path);
        std::cout << "[AssetPipeline] Model uploaded to GPU: " << path << std::endl;

        // --------- Auto-generate humanoid avatar (heuristic) ---------
        // Use the runtime scene loader path to build skeleton bind data, then export an .avatar file next to the model.
        try {
            // Build a transient skeleton with parents using Assimp hierarchy for accurate bind locals
            Assimp::Importer aimport;
            const aiScene* aScene = aimport.ReadFile(path,
                aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace |
                aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices |
                aiProcess_ImproveCacheLocality | aiProcess_LimitBoneWeights);
            SkeletonComponent tempSkel;
            tempSkel.InverseBindPoses = model.InverseBindPoses;
            tempSkel.BoneParents.resize(model.BoneNames.size(), -1);
            for (int i = 0; i < (int)model.BoneNames.size(); ++i) tempSkel.BoneNameToIndex[model.BoneNames[i]] = i;

            if (aScene && aScene->mRootNode) {
                std::unordered_map<std::string, aiNode*> nodeByName;
                std::function<void(aiNode*)> gather = [&](aiNode* n){ nodeByName[n->mName.C_Str()] = n; for (unsigned c=0;c<n->mNumChildren;++c) gather(n->mChildren[c]); };
                gather(aScene->mRootNode);
                // Build name->index map
                std::unordered_map<std::string, int> boneIndexMap;
                for (int i = 0; i < (int)model.BoneNames.size(); ++i) boneIndexMap[model.BoneNames[i]] = i;
                for (size_t i = 0; i < model.BoneNames.size(); ++i) {
                    auto itNode = nodeByName.find(model.BoneNames[i]);
                    if (itNode != nodeByName.end()) {
                        aiNode* p = itNode->second->mParent;
                        while (p) {
                            auto itBI = boneIndexMap.find(p->mName.C_Str());
                            if (itBI != boneIndexMap.end()) { tempSkel.BoneParents[i] = itBI->second; break; }
                            p = p->mParent;
                        }
                    }
                }
            }

            cm::animation::AvatarDefinition avatar;
            avatar.RigName = std::filesystem::path(path).stem().string();
            cm::animation::avatar_builders::BuildFromSkeleton(tempSkel, avatar, true);
            // Save next to the model
            std::filesystem::path p(path);
            std::string avatarPath = (p.parent_path() / (p.stem().string() + ".avatar")).string();
            cm::animation::SaveAvatar(avatar, avatarPath);
            std::cout << "[AssetPipeline] Wrote avatar: " << avatarPath << std::endl;
        } catch(...) {
            // Non-fatal
        } 

        // --------- Extract animations (unified .anim as primary) ---------
        using namespace cm::animation;
        auto clips = AnimationImporter::ImportFromModel(path);
        std::cout << "[AssetPipeline] ImportFromModel found " << clips.size() << " animation(s)." << std::endl;
        if (!clips.empty()) {
            std::filesystem::path p(path);
            std::string dir = p.parent_path().string();
            // Try to load/create a source avatar for this rig
            AvatarDefinition srcAvatar;
            std::string avatarPath = (p.parent_path() / (p.stem().string() + ".avatar")).string();
            bool hasAvatar = cm::animation::LoadAvatar(srcAvatar, avatarPath);
            for (auto& clip : clips) {
                // Build a unified AnimationAsset and convert skeletal to Avatar tracks if humanoid
                AnimationAsset asset; asset.name = clip.Name; asset.meta.version = 1; asset.meta.fps = (clip.TicksPerSecond > 0.0f ? clip.TicksPerSecond : 30.0f); asset.meta.length = clip.Duration;
                bool isHumanoid = false;
                if (hasAvatar) {
                    // Convert using avatar mapping when bone names match, otherwise fall back to skeletal
                    for (const auto& [boneName, bt] : clip.BoneTracks) {
                        // Find mapped humanoid id
                        int mappedId = -1;
                        for (uint16_t i = 0; i < cm::animation::HumanoidBoneCount; ++i) {
                            if (!srcAvatar.Present[i]) continue;
                            const auto& e = srcAvatar.Map[i];
                            if (!e.BoneName.empty() && e.BoneName == boneName) { mappedId = (int)i; break; }
                        }
                        if (mappedId >= 0) {
                            isHumanoid = true;
                            auto t = std::make_unique<AssetAvatarTrack>();
                            t->humanBoneId = mappedId;
                            t->name = std::string("Humanoid:") + ToString(static_cast<cm::animation::HumanoidBone>(mappedId));
                            for (const auto& k : bt.PositionKeys) t->t.keys.push_back({0ull, k.Time, k.Value});
                            for (const auto& k : bt.RotationKeys) t->r.keys.push_back({0ull, k.Time, k.Value});
                            for (const auto& k : bt.ScaleKeys)    t->s.keys.push_back({0ull, k.Time, k.Value});
                            asset.tracks.push_back(std::move(t));
                        }
                    }
                }
                // If not humanoid or no avatar mapping, keep skeletal bone tracks
                if (!isHumanoid) {
                    for (const auto& [boneName, bt] : clip.BoneTracks) {
                        auto t = std::make_unique<AssetBoneTrack>();
                        t->name = boneName;
                        for (const auto& k : bt.PositionKeys) t->t.keys.push_back({0ull, k.Time, k.Value});
                        for (const auto& k : bt.RotationKeys) t->r.keys.push_back({0ull, k.Time, k.Value});
                        for (const auto& k : bt.ScaleKeys)    t->s.keys.push_back({0ull, k.Time, k.Value});
                        asset.tracks.push_back(std::move(t));
                    }
                }

                // Save as unified .anim
                std::string outPath = dir + "/" + p.stem().string() + "_" + clip.Name + ".anim";
                if (SaveAnimationAsset(asset, outPath)) {
                    std::cout << "[AssetPipeline] Saved animation asset: " << outPath << std::endl;
                }
            }
        }
        });
}

// ---------------------------------------
// Enqueue Model Import (BG job → main-thread callback)
// ---------------------------------------
void AssetPipeline::EnqueueModelImport(const ImportRequest& req) {
    // Run CPU-heavy build in a background job; then marshal to main thread
    Jobs().Enqueue([this, req]{
        try {
            BuiltModelPaths built{};
            if (!EnsureModelCache(req.sourcePath, built)) {
                EnqueueMainThreadTask([cb = req.onReady]{ if (cb) { BuiltModelPaths empty{}; cb(empty); } });
                return;
            }
            EnqueueMainThreadTask([built, cb = req.onReady]{ if (cb) cb(built); });
        } catch (const std::exception& e) {
            EnqueueMainThreadTask([cb = req.onReady, msg = std::string(e.what())]{
                std::cerr << "[AssetPipeline] Import job threw: " << msg << std::endl;
                if (cb) { BuiltModelPaths empty{}; cb(empty); }
            });
        } catch (...) {
            EnqueueMainThreadTask([cb = req.onReady]{
                std::cerr << "[AssetPipeline] Import job threw unknown exception" << std::endl;
                if (cb) { BuiltModelPaths empty{}; cb(empty); }
            });
        }
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
        ".fbx", ".obj", ".gltf", ".glb", // Models
        ".png", ".jpg", ".jpeg", ".tga", // Textures
        ".sc", ".shader", ".glsl",         // Shaders
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

// -----------------------------------------------------
// FIXUP GUID REFERENCES in scenes/prefabs after reimport
// -----------------------------------------------------
void AssetPipeline::FixupAssetReferencesByName(const std::string& projectRoot) {
    namespace fs = std::filesystem;
    // Build lookups:
    // - filename (without dir) -> {GUID, virtual path}
    // - filename -> virtual path (for non-GUID assets like textures)
    std::unordered_map<std::string, std::pair<ClaymoreGUID, std::string>> nameToGuidPath;
    std::unordered_map<std::string, std::string> nameToVPath;
    // Walk assets dir
    fs::path assetsDir = fs::path(projectRoot) / "assets";
    if (fs::exists(assetsDir)) {
        for (auto& e : fs::recursive_directory_iterator(assetsDir)) {
            if (!e.is_regular_file()) continue;
            std::string p = e.path().string();
            const AssetMetadata* meta = AssetRegistry::Instance().GetMetadata(p);
            std::string v = p; std::replace(v.begin(), v.end(), '\\', '/');
            auto pos = v.find("assets/"); if (pos != std::string::npos) v = v.substr(pos);
            const std::string fname = e.path().filename().string();
            nameToVPath[fname] = v;
            if (meta && !(meta->guid.high == 0 && meta->guid.low == 0)) {
                nameToGuidPath[fname] = { meta->guid, v };
            }
        }
    }

    auto tryFixFile = [&](const fs::path& path) {
        try {
            std::ifstream in(path.string());
            if (!in.is_open()) return;
            nlohmann::json j; in >> j; in.close();
            bool changed = false;
            // Walk entities -> mesh and material/anim paths
            if (j.contains("entities") && j["entities"].is_array()) {
                for (auto& ent : j["entities"]) {
                    if (ent.contains("mesh") && ent["mesh"].is_object()) {
                        auto& m = ent["mesh"];
                        // If meshReference guid is zero, or guid not known, try resolve by filename from meshPath
                        bool need = false;
                        ClaymoreGUID g{};
                        if (m.contains("meshReference") && m["meshReference"].contains("guid")) {
                            std::string gs = m["meshReference"]["guid"].get<std::string>();
                            g = ClaymoreGUID::FromString(gs);
                            if (g.high == 0 && g.low == 0) need = true;
                        } else need = true;
                        std::string filename;
                        if (m.contains("meshPath")) filename = fs::path(m["meshPath"].get<std::string>()).filename().string();
                        else if (m.contains("meshName")) filename = m["meshName"].get<std::string>();
                        if (need && !filename.empty()) {
                            auto it = nameToGuidPath.find(filename);
                            if (it != nameToGuidPath.end()) {
                                m["meshReference"]["guid"] = it->second.first.ToString();
                                m["meshReference"]["type"] = (int)AssetType::Mesh;
                                if (!m.contains("meshPath")) m["meshPath"] = it->second.second;
                                changed = true;
                            }
                        }

                        // Normalize material texture paths to virtual assets path if missing or invalid
                        auto fixTex = [&](const char* key){
                            if (!m.contains(key)) return;
                            std::string val = m[key].get<std::string>();
                            std::string fname = fs::path(val).filename().string();
                            // If not under assets/ or file not present, map by filename
                            bool needsMap = val.empty() || val.find("assets/") == std::string::npos;
                            if (!needsMap) {
                                // keep
                            } else if (!fname.empty()) {
                                auto it2 = nameToVPath.find(fname);
                                if (it2 != nameToVPath.end()) { m[key] = it2->second; changed = true; }
                            }
                        };
                        fixTex("mat_albedoPath");
                        fixTex("mat_mrPath");
                        fixTex("mat_normalPath");

                        // PropertyBlock texture overrides
                        if (m.contains("propertyBlockTextures") && m["propertyBlockTextures"].is_object()) {
                            for (auto it = m["propertyBlockTextures"].begin(); it != m["propertyBlockTextures"].end(); ++it) {
                                std::string val = it.value().get<std::string>();
                                std::string fname = fs::path(val).filename().string();
                                if (!fname.empty()) {
                                    auto it2 = nameToVPath.find(fname);
                                    if (it2 != nameToVPath.end()) { it.value() = it2->second; changed = true; }
                                }
                            }
                        }
                    }

                    // Animator controller/clip paths normalization
                    if (ent.contains("animator") && ent["animator"].is_object()) {
                        auto& a = ent["animator"];
                        auto normAsset = [&](const char* key){
                            if (!a.contains(key)) return;
                            std::string val = a[key].get<std::string>();
                            std::string fname = fs::path(val).filename().string();
                            if (!fname.empty()) {
                                auto it2 = nameToVPath.find(fname);
                                if (it2 != nameToVPath.end()) { a[key] = it2->second; changed = true; }
                            }
                        };
                        normAsset("controllerPath");
                        normAsset("singleClipPath");
                    }
                }
            }
            if (changed) {
                std::ofstream out(path.string());
                out << j.dump(4);
            }
        } catch (...) { }
    };

    // Helper: robust directory walk (skip permission errors)
    auto walkDir = [&](const fs::path& dir, const std::string& ext){
        std::error_code ec;
        if (!fs::exists(dir, ec)) return;
        fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
        while (!ec && it != end) {
            if (it->is_regular_file(ec) && it->path().extension() == ext) tryFixFile(it->path());
            it.increment(ec);
        }
    };

    // Fix scenes under <projectRoot>/scenes
    walkDir(fs::path(projectRoot) / "scenes", ".scene");
    // Fix prefabs under <projectRoot>/assets
    walkDir(fs::path(projectRoot) / "assets", ".prefab");
}
