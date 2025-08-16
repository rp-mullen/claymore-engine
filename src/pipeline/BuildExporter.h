#pragma once
#include <string>
#include <vector>

// BuildExporter: collects required files (scenes, prefabs, textures, models, shaders, scripts)
// and writes a single .pak file alongside a stripped runtime.

class BuildExporter {
public:
    struct Options {
        std::string outputDirectory; // where to place MyGame.exe and MyGame.pak
        std::vector<std::string> entryScenes; // absolute or project-relative scene paths to include
        bool includeAllAssets = false; // debug switch
    };

    // High-level: export current project as standalone
    static bool ExportProject(const Options& opts);
    static void AddIfExists(const std::string& path,
        std::vector<std::string>& outFiles);

private:
    static void CollectSceneDependencies(const std::string& scenePath,
                                         std::vector<std::string>& outFiles);
    
};


