#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace ProjectGenerator {

    bool CreateNewProject(const std::string& name, const std::filesystem::path& targetDir) {
        using namespace std::filesystem;

        path projectRoot = targetDir / name;
        if (exists(projectRoot)) {
            std::cerr << "[ProjectGenerator] Folder already exists: " << projectRoot << std::endl;
            return false;
        }

        create_directories(projectRoot / "assets/textures");
        create_directories(projectRoot / "assets/models");
        create_directories(projectRoot / "assets/materials");
        create_directories(projectRoot / "scenes");
        create_directories(projectRoot / "scripts");
        create_directories(projectRoot / "shaders");

        // Generate .clayproj
        nlohmann::json j;
        j["name"] = name;
        j["version"] = 1;
        j["assetDirectory"] = "assets";
        j["startScene"] = "scenes/main.scene";
        j["renderer"] = {
            { "api", "Direct3D11" },
            { "vSync", true }
        };

        std::ofstream out(projectRoot / (name + ".clayproj"));
        out << j.dump(4);
        out.close();

        // Optional: create empty main.scene file
        std::ofstream mainScene(projectRoot / "scenes/main.scene");
        mainScene << "{}";
        mainScene.close();

        std::cout << "[ProjectGenerator] Created new project at " << projectRoot << std::endl;
        return true;
    }

} // namespace ProjectGenerator
