#include "BuildExporter.h"
#include "PakArchive.h"
#include "AssetMetadata.h"
#include "AssetRegistry.h"
#include <serialization/Serializer.h>
#include <editor/Project.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <unordered_set>

#include "pipeline/AssetLibrary.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

static bool CopyDirectoryRecursive(const fs::path& src, const fs::path& dst) {
    std::error_code ec;
    if (!fs::exists(src)) return false;
    for (auto it = fs::recursive_directory_iterator(src, ec); !ec && it != fs::recursive_directory_iterator(); ++it) {
        const auto& entry = *it;
        fs::path rel = fs::relative(entry.path(), src, ec);
        if (ec) rel = entry.path().filename();
        fs::path outPath = dst / rel;
        if (entry.is_directory()) {
            fs::create_directories(outPath, ec);
        } else if (entry.is_regular_file()) {
            fs::create_directories(outPath.parent_path(), ec);
            std::error_code cec; fs::copy_file(entry.path(), outPath, fs::copy_options::overwrite_existing, cec);
        }
    }
    return true;
}

static std::string MakeVirtualPath(const fs::path& absPath) {
    // Normalize to forward slashes first
    std::string s = absPath.generic_string();
    for (char &c : s) if (c == '\\') c = '/';

    // If the absolute path contains an 'assets/' or 'shaders/' segment anywhere,
    // use that segment and everything after it as the virtual path.
    auto posAssets = s.find("assets/");
    if (posAssets != std::string::npos) return s.substr(posAssets);
    auto posShaders = s.find("shaders/");
    if (posShaders != std::string::npos) return s.substr(posShaders);

    // Otherwise try to make it relative to the project directory
    fs::path proj = Project::GetProjectDirectory();
    std::error_code ec;
    fs::path rel = fs::relative(absPath, proj, ec);
    std::string out = (ec ? s : rel.generic_string());
    for (char &c : out) if (c == '\\') c = '/';
    return out;
}

void BuildExporter::AddIfExists(const std::string& path, std::vector<std::string>& outFiles) {
    if (path.empty()) return;
    if (fs::exists(path)) outFiles.push_back(path);
}

static const std::vector<std::string> kExts = {
    ".fbx", ".obj", ".gltf", ".glb",
    ".png", ".jpg", ".jpeg", ".tga",
    ".anim", ".avatar", ".controller",
    ".wav", ".mp3", ".ogg",
    ".ttf", ".otf",
    ".cs", ".dll",
    ".mat", ".json", ".prefab"
};

static bool LooksLikeAssetPath(const std::string& s) {
    fs::path p(s);
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return std::find(kExts.begin(), kExts.end(), ext) != kExts.end();
}

static void CollectPathsFromJson(const json& j, std::vector<std::string>& outFiles) {
    if (j.is_string()) {
        std::string v = j.get<std::string>();
        if (LooksLikeAssetPath(v)) BuildExporter::AddIfExists(v, outFiles);
    } else if (j.is_array()) {
        for (auto& e : j) CollectPathsFromJson(e, outFiles);
    } else if (j.is_object()) {
        // Special-case meshReference.guid -> resolve to path via AssetLibrary for inclusion
        if (j.contains("meshReference")) {
            const json& r = j["meshReference"];
            if (r.contains("guid") && r["guid"].is_string()) {
                ClaymoreGUID g = ClaymoreGUID::FromString(r["guid"].get<std::string>());
                std::string p = AssetLibrary::Instance().GetPathForGUID(g);
                if (!p.empty()) BuildExporter::AddIfExists(p, outFiles);
            }
        }
        for (auto it = j.begin(); it != j.end(); ++it) CollectPathsFromJson(it.value(), outFiles);
    }
}

void BuildExporter::CollectSceneDependencies(const std::string& scenePath,
                                             std::vector<std::string>& outFiles)
{
    AddIfExists(scenePath, outFiles);
    std::ifstream in(scenePath);
    if (!in.is_open()) return;
    json j; in >> j;
    CollectPathsFromJson(j, outFiles);
}

bool BuildExporter::ExportProject(const Options& opts) {
    if (opts.entryScenes.empty()) {
        std::cerr << "[BuildExporter] ERROR: No entry scene specified. Aborting export." << std::endl;
        return false;
    }
    std::unordered_set<std::string> uniqueFiles;
    std::vector<std::string> files;

    // Scenes chosen by user
    for (const auto& s : opts.entryScenes) CollectSceneDependencies(s, files);

    // Build GUID->path map from current AssetLibrary/Registry and include it
    json assetMap = json::array();
    {
        // Iterate the registry if available on disk; otherwise scan assets folder
        fs::path proj = Project::GetProjectDirectory();
        fs::path assets = proj / "assets";
        if (fs::exists(assets)) {
            for (auto& e : fs::recursive_directory_iterator(assets)) {
                if (!e.is_regular_file()) continue;
                std::string p = e.path().string();
                // Try to get a GUID for this path if it exists in registry
                const AssetMetadata* meta = AssetRegistry::Instance().GetMetadata(p);
                if (meta && meta->guid != ClaymoreGUID()) {
                    json rec;
                    rec["guid"] = meta->guid.ToString();
                    // Write virtual path for runtime
                    rec["path"] = MakeVirtualPath(e.path());
                    assetMap.push_back(rec);
                }
            }
        }
    }

    // Always include compiled shader bins only (not sources) for runtime
    fs::path exeDir = fs::current_path();
    fs::path compiledDir = exeDir / "shaders" / "compiled" / "windows";
    if (fs::exists(compiledDir)) {
        for (auto& e : fs::recursive_directory_iterator(compiledDir)) {
            if (e.is_regular_file() && e.path().extension() == ".bin") files.push_back(e.path().string());
        }
    }
    // Also include directly any pre-existing .bin in shaders/ for safety
    fs::path flatDir = exeDir / "shaders";
    if (fs::exists(flatDir)) {
        for (auto& e : fs::directory_iterator(flatDir)) {
            if (e.is_regular_file() && e.path().extension() == ".bin") files.push_back(e.path().string());
        }
    }

    // Minimal runtime asset set required by renderer/text
    {
        const char* kRuntimeAssets[] = {
            "assets/debug/white.png",
            "assets/debug/metallic_roughness.png",
            "assets/debug/normal.png",
            "assets/fonts/Roboto-Regular.ttf"
        };
        fs::path proj = Project::GetProjectDirectory();
        // Try project assets first, then fallback to engine-level assets under repo root (parent of ClayProject)
        fs::path repoRoot = proj.parent_path();
        // Final fallback: derive from exe dir
        if (repoRoot.empty() || !fs::exists(repoRoot)) {
            fs::path exeDirHere = fs::current_path();
            repoRoot = fs::weakly_canonical(exeDirHere / "../../..");
        }
        for (const char* rel : kRuntimeAssets) {
            bool added = false;
            fs::path absProj = proj / rel;
            if (fs::exists(absProj)) { AddIfExists(absProj.string(), files); added = true; }
            if (!added) {
                fs::path absEngine = repoRoot / rel; // e.g., <repo>/assets/debug/white.png
                AddIfExists(absEngine.string(), files);
            }
        }
    }

    // Always include assets that are directly referenced in project panel? Optional debug include
    if (opts.includeAllAssets) {
        fs::path assets = Project::GetAssetDirectory();
        if (fs::exists(assets)) {
            for (auto& e : fs::recursive_directory_iterator(assets)) {
                if (e.is_regular_file()) files.push_back(e.path().string());
            }
        }
    }

    // Dedup
    std::vector<std::string> dedup;
    for (auto& f : files) {
        if (uniqueFiles.insert(f).second) dedup.push_back(f);
    }

    // Build .pak
    PakArchive pak;
    // Prepare manifest with entry scene virtual path (if any)
    std::string entrySceneVPath;
    if (!opts.entryScenes.empty()) {
        const std::string& first = opts.entryScenes.front();
        entrySceneVPath = MakeVirtualPath(fs::path(first));
    }
    for (const auto& f : dedup) {
        std::ifstream in(f, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "[BuildExporter] Warning: Could not open file: " << f << std::endl;
            continue;
        }
        in.seekg(0, std::ios::end);
        size_t size = static_cast<size_t>(in.tellg());
        in.seekg(0, std::ios::beg);
        std::vector<uint8_t> data(size);
        if (size != 0) in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));

        // Virtual path within pak
        std::string vpath = MakeVirtualPath(fs::path(f));
        pak.AddFile(vpath, data);
        std::cout << "[BuildExporter] Added to pak: " << vpath << " (" << size << " bytes)" << std::endl;
    }

    // Add simple manifest
    {
        json manifest;
        manifest["entryScene"] = entrySceneVPath;
        if (!assetMap.empty()) manifest["assetMap"] = assetMap;
        std::string text = manifest.dump(0);
        std::vector<uint8_t> bytes(text.begin(), text.end());
        pak.AddFile("game_manifest.json", bytes);
    }

    fs::create_directories(opts.outputDirectory);
    // Resolve project name with sensible fallback
    std::string projName = Project::GetProjectName();
    if (projName.empty()) {
        fs::path projDir = Project::GetProjectDirectory();
        if (!projDir.empty()) projName = projDir.filename().string();
        if (projName.empty()) projName = "Game";
    }
    fs::path pakOut = fs::path(opts.outputDirectory) / (projName + ".pak");
    if (!pak.SaveToFile(pakOut.string())) return false;

    // Copy runtime executable and required DLLs next to pak, configured via manifest
    fs::path runtimeDir = exeDir;
    std::vector<std::string> runtimeFiles;

    fs::path manifestJson = exeDir / "tools" / "runtime_manifest.json";
    if (fs::exists(manifestJson)) {
        try {
            std::ifstream mf(manifestJson.string());
            json mj; mf >> mj;
            if (mj.contains("binaries") && mj["binaries"].is_array()) {
                for (auto& v : mj["binaries"]) if (v.is_string()) runtimeFiles.push_back(v.get<std::string>());
            }
        } catch(...) {}
    } else {
        runtimeFiles = { "Claymore.exe", "nethost.dll", "ClaymoreEngine.dll", "GameScripts.dll" };
    }

    for (const auto& rf : runtimeFiles) {
        fs::path src = runtimeDir / rf;
        fs::path dst = fs::path(opts.outputDirectory) / rf;
        if (fs::exists(src)) {
            try { 
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing); 
                std::cout << "[BuildExporter] Copied runtime file: " << rf << std::endl;
            } catch(const std::exception& e) {
                std::cerr << "[BuildExporter] Failed to copy runtime file " << rf << ": " << e.what() << std::endl;
            }
        } else {
            std::cerr << "[BuildExporter] Warning: Runtime file not found: " << src << std::endl;
        }
    }

    // Also copy the managed engine output directory (scripts/ClaymoreEngine/bin/<Config>/<TFM>) into the export
    try {
        // Heuristic to resolve repo root from current exe dir (../../.. relative like Application does for ClayProject)
        fs::path repoRoot = fs::weakly_canonical(exeDir / "../../..");
        std::vector<fs::path> managedCandidates = {
            repoRoot / "scripts/ClaymoreEngine/bin/Debug/net8.0",
            repoRoot / "scripts/ClaymoreEngine/bin/Debug/net8.0-windows",
            repoRoot / "scripts/ClaymoreEngine/bin/Release/net8.0",
            repoRoot / "scripts/ClaymoreEngine/bin/Release/net8.0-windows"
        };
        fs::path chosen;
        for (const auto& c : managedCandidates) { if (fs::exists(c)) { chosen = c; break; } }
        if (!chosen.empty()) {
            std::cout << "[BuildExporter] Copying managed runtime from: " << chosen << std::endl;
            CopyDirectoryRecursive(chosen, fs::path(opts.outputDirectory));
        } else {
            std::cerr << "[BuildExporter] Warning: Managed output directory not found under scripts/ClaymoreEngine/bin." << std::endl;
        }
    } catch(const std::exception& e) {
        std::cerr << "[BuildExporter] Failed copying managed runtime directory: " << e.what() << std::endl;
    }

    // Optionally rename executable to project name for convenience
    try {
        const std::string proj = projName;
        fs::path dstExe = fs::path(opts.outputDirectory) / (proj + ".exe");
        fs::path srcExe = fs::path(opts.outputDirectory) / "Claymore.exe";
        if (fs::exists(srcExe)) {
            fs::copy_file(srcExe, dstExe, fs::copy_options::overwrite_existing);
            std::cout << "[BuildExporter] Created project executable: " << (proj + ".exe") << std::endl;
        }
    } catch(const std::exception& e) {
        std::cerr << "[BuildExporter] Failed to create project executable: " << e.what() << std::endl;
    }

    // Drop a marker file to force play-mode in exported builds
    try {
        std::ofstream marker(fs::path(opts.outputDirectory) / "game_mode_only.marker", std::ios::trunc);
        marker << "play_mode_only";
        marker.close();
    } catch(...) {}

    std::cout << "[BuildExporter] Export completed successfully!" << std::endl;
    std::cout << "[BuildExporter] Output directory: " << opts.outputDirectory << std::endl;
    std::cout << "[BuildExporter] Files included: " << dedup.size() << std::endl;
    std::cout << "[BuildExporter] Runtime files copied: " << runtimeFiles.size() << std::endl;

    return true;
}


