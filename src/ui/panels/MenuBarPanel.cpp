#include "MenuBarPanel.h"
#include "ecs/Entity.h"
#include "ecs/Components.h"
#include <editor/Project.h>
#include "serialization/Serializer.h"
#include "ecs/EntityData.h"
#include "ui/UILayer.h"
#include "pipeline/AssetPipeline.h"
#include "pipeline/AssetRegistry.h"
#include "pipeline/AssetLibrary.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>  // JSON serialization
#include <rendering/StandardMeshManager.h>
#include <rendering/MaterialManager.h>
#include <rendering/Environment.h>
#include <Windows.h>

using json = nlohmann::json;

static std::string WideToUtf8(const std::wstring& wide)
{
    if (wide.empty()) return {};
    int requiredBytes = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (requiredBytes <= 0) return {};
    std::string utf8(static_cast<size_t>(requiredBytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), requiredBytes, nullptr, nullptr);
    return utf8;
}

static std::wstring Utf8ToWide(const std::string& utf8)
{
    if (utf8.empty()) return {};
    int requiredWchars = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (requiredWchars <= 0) return {};
    std::wstring wide(static_cast<size_t>(requiredWchars - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), requiredWchars);
    return wide;
}

std::string ShowOpenFolderDialog() {
    IFileDialog* pFileDialog;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileDialog));
    if (FAILED(hr)) return "";

    DWORD options;
    pFileDialog->GetOptions(&options);
    pFileDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

    hr = pFileDialog->Show(nullptr);
    if (SUCCEEDED(hr)) {
        IShellItem* pItem;
        hr = pFileDialog->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR path;
            pItem->GetDisplayName(SIGDN_FILESYSPATH, &path);
            std::wstring ws(path);
            CoTaskMemFree(path);
            pItem->Release();
            pFileDialog->Release();
            return WideToUtf8(ws);
        }
    }
    pFileDialog->Release();
    return "";
}

std::string ShowSaveFileDialog(const std::string& defaultName = "scene.scene") {
    IFileDialog* pFileDialog;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileDialog));
    if (FAILED(hr)) return "";

    // Set file type filter
    COMDLG_FILTERSPEC filterSpec[] = {
        { L"Scene Files", L"*.scene" },
        { L"All Files", L"*.*" }
    };
    pFileDialog->SetFileTypes(2, filterSpec);
    pFileDialog->SetDefaultExtension(L"scene");

    // Set default file name
    std::wstring defaultNameW = Utf8ToWide(defaultName);
    pFileDialog->SetFileName(defaultNameW.c_str());

    hr = pFileDialog->Show(nullptr);
    if (SUCCEEDED(hr)) {
        IShellItem* pItem;
        hr = pFileDialog->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR path;
            pItem->GetDisplayName(SIGDN_FILESYSPATH, &path);
            std::wstring ws(path);
            CoTaskMemFree(path);
            pItem->Release();
            pFileDialog->Release();
            return WideToUtf8(ws);
        }
    }

    pFileDialog->Release();
    return "";
}

std::string ShowOpenFileDialog() {
    IFileDialog* pFileDialog;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileDialog));
    if (FAILED(hr)) return "";

    // Set file type filter
    COMDLG_FILTERSPEC filterSpec[] = {
        { L"Scene Files", L"*.scene" },
        { L"All Files", L"*.*" }
    };
    pFileDialog->SetFileTypes(2, filterSpec);

    hr = pFileDialog->Show(nullptr);
    if (SUCCEEDED(hr)) {
        IShellItem* pItem;
        hr = pFileDialog->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR path;
            pItem->GetDisplayName(SIGDN_FILESYSPATH, &path);
            std::wstring ws(path);
            CoTaskMemFree(path);
            pItem->Release();
            pFileDialog->Release();
            return WideToUtf8(ws);
        }
    }

    pFileDialog->Release();
    return "";
}

void MenuBarPanel::OnImGuiRender() {
    // FILE MENU
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene")) {
            *m_Context = Scene();
            *m_SelectedEntity = -1;
        }

        ImGui::Separator();
        
        if (ImGui::MenuItem("Save Scene")) {
            // TODO: Save to current scene file if one exists, otherwise show save dialog
            std::string scenePath = "assets/scenes/CurrentScene.scene";
            if (Serializer::SaveSceneToFile(*m_Context, scenePath)) {
                std::cout << "[MenuBarPanel] Scene saved successfully to: " << scenePath << std::endl;
            } else {
                std::cerr << "[MenuBarPanel] Failed to save scene to: " << scenePath << std::endl;
            }
        }
        
        if (ImGui::MenuItem("Save Scene As...")) {
            std::string scenePath = ShowSaveFileDialog("NewScene.scene");
            if (!scenePath.empty()) {
                if (Serializer::SaveSceneToFile(*m_Context, scenePath)) {
                    std::cout << "[MenuBarPanel] Scene saved successfully to: " << scenePath << std::endl;
                } else {
                    std::cerr << "[MenuBarPanel] Failed to save scene to: " << scenePath << std::endl;
                }
            }
        }
        
        if (ImGui::MenuItem("Load Scene...")) {
            std::string scenePath = ShowOpenFileDialog();
            if (!scenePath.empty()) {
                if (m_UILayer) {
                    // Use deferred scene loading to avoid race conditions
                    m_UILayer->DeferSceneLoad(scenePath);
                } else {
                    // Fallback to immediate loading if UILayer not available
                    if (Serializer::LoadSceneFromFile(scenePath, *m_Context)) {
                        std::cout << "[MenuBarPanel] Scene loaded successfully from: " << scenePath << std::endl;
                        *m_SelectedEntity = -1; // Clear selection
                    } else {
                        std::cerr << "[MenuBarPanel] Failed to load scene from: " << scenePath << std::endl;
                    }
                }
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Open Project...")) {
            std::string path = ShowOpenFolderDialog();
            if (!path.empty()) {
                std::filesystem::path projPath = path;
                std::filesystem::path clayprojFile;

                // Look for existing .clayproj file
                for (const auto& entry : std::filesystem::directory_iterator(projPath)) {
                    if (entry.path().extension() == ".clayproj") {
                        clayprojFile = entry.path();
                        break;
                    }
                }

                // If no .clayproj found, create one
                if (clayprojFile.empty()) {
                    clayprojFile = projPath / (projPath.filename().string() + ".clayproj");

                    json projectJson = {
                        { "name", projPath.filename().string() },
                        { "version", "1.0" },
                        { "scenes", json::array() }
                    };

                    std::ofstream outFile(clayprojFile);
                    if (!outFile) {
                        std::cerr << "[MenuBarPanel] Failed to create project file: " << clayprojFile << std::endl;
                        return;
                    }

                    outFile << projectJson.dump(4); // Pretty print with indent
                    outFile.close();

                    std::cout << "[MenuBarPanel] Created new .clayproj file: " << clayprojFile << std::endl;
                }
                else {
                    std::cout << "[MenuBarPanel] Found existing project file: " << clayprojFile << std::endl;
                }

                Project::Load(clayprojFile.string());
                m_ProjectPanel->LoadProject(projPath.string());
            }
        }

        if (ImGui::MenuItem("Exit")) {
            // Hook into application quit logic
        }
        ImGui::EndMenu();
    }

    // SCENE MENU
    if (ImGui::BeginMenu("Scene")) {
        if (ImGui::BeginMenu("Environment")) {
            Environment& env = m_Context->GetEnvironment();
            ImGui::Checkbox("Enable Fog", &env.EnableFog);
            ImGui::ColorEdit3("Fog Color", (float*)&env.FogColor);
            ImGui::SliderFloat("Fog Density", &env.FogDensity, 0.0f, 0.2f, "%.3f");
            ImGui::Separator();
            ImGui::ColorEdit3("Ambient Color", (float*)&env.AmbientColor);
            ImGui::SliderFloat("Ambient Intensity", &env.AmbientIntensity, 0.0f, 5.0f, "%.2f");
            ImGui::Separator();
            ImGui::SliderFloat("Exposure", &env.Exposure, 0.1f, 5.0f, "%.2f");
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    // TOOLS MENU
    if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("Reimport Assets")) {
            namespace fs = std::filesystem;
            std::string projectDir = Project::GetProjectDirectory().string();

            // 1. Delete all .meta files in project directory
            for (auto& entry : fs::recursive_directory_iterator(projectDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".meta") {
                    try {
                        fs::remove(entry.path());
                    } catch (const std::exception& e) {
                        std::cerr << "[ReimportAssets] Failed to delete meta file: " << entry.path() << "\n";
                    }
                }
            }

            // 2. Optionally clear cached processed assets folder
            if (fs::exists("cache")) {
                try { fs::remove_all("cache"); }
                catch(...) {}
            }

            // 3. Clear registries so they repopulate on import
            AssetRegistry::Instance().Clear();
            AssetLibrary::Instance().Clear();

            // 4. Scan and import all assets anew (this will compile scripts as well)
            AssetPipeline::Instance().ScanProject(projectDir);
            std::cout << "[MenuBarPanel] Reimport Assets triggered." << std::endl;
        }

        if (ImGui::MenuItem("Reimport Scripts")) {
            namespace fs = std::filesystem;
            std::string projectDir = Project::GetProjectDirectory().string();

            // 1. Delete .meta files for scripts and collect first script path
            std::string firstScriptPath;
            for (auto& entry : fs::recursive_directory_iterator(projectDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".cs") {
                    fs::path meta = entry.path();
                    meta += ".meta";
                    if (fs::exists(meta)) {
                        try { fs::remove(meta); } catch(...) {}
                    }

                    AssetRegistry::Instance().RemoveMetadata(entry.path().string());

                    if (firstScriptPath.empty())
                        firstScriptPath = entry.path().string();
                }
            }

            // 2. Remove existing compiled DLL to force rebuild (if present)
            try {
                fs::path exeDir = std::filesystem::current_path();
                fs::path dllPath = exeDir / "GameScripts.dll";
                if (fs::exists(dllPath)) fs::remove(dllPath);
            } catch(...) {}

            // 3. Recompile scripts if any found
            if (!firstScriptPath.empty()) {
                AssetPipeline::Instance().ImportScript(firstScriptPath);
                std::cout << "[MenuBarPanel] Reimport Scripts triggered." << std::endl;
            } else {
                std::cerr << "[MenuBarPanel] No .cs scripts found in project." << std::endl;
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) {
            if (m_UILayer) m_UILayer->RequestLayoutReset();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("UI Scale +")) {
            ImGuiIO& io = ImGui::GetIO();
            io.FontGlobalScale = std::min(2.0f, io.FontGlobalScale + 0.1f);
        }
        if (ImGui::MenuItem("UI Scale -")) {
            ImGuiIO& io = ImGui::GetIO();
            io.FontGlobalScale = std::max(0.5f, io.FontGlobalScale - 0.1f);
        }
        ImGui::EndMenu();
    }

    // ENTITY MENU
    if (ImGui::BeginMenu("Entity")) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Empty")) {
                auto entity = m_Context->CreateEntity("Empty Entity");
                *m_SelectedEntity = entity.GetID();
            }

            if (ImGui::MenuItem("Camera")) {
                auto entity = m_Context->CreateEntity("Camera");
                if (auto* data = m_Context->GetEntityData(entity.GetID())) {
                    data->Camera = new CameraComponent();
                }
                *m_SelectedEntity = entity.GetID();
            }

			if (ImGui::MenuItem("Cube")) {
				auto entity = m_Context->CreateEntity("Cube");
				if (auto* data = m_Context->GetEntityData(entity.GetID())) {
					data->Mesh = new MeshComponent();
					data->Mesh->mesh = StandardMeshManager::Instance().GetCubeMesh();
					data->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();
					data->Mesh->MeshName = "Cube";
				}
				*m_SelectedEntity = entity.GetID();
			}

			if (ImGui::MenuItem("Plane")) {
				auto entity = m_Context->CreateEntity("Plane");
				if (auto* data = m_Context->GetEntityData(entity.GetID())) {
					data->Mesh = new MeshComponent();
					data->Mesh->mesh = StandardMeshManager::Instance().GetPlaneMesh();
					data->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();
					data->Mesh->MeshName = "Plane";
				}
				*m_SelectedEntity = entity.GetID();
			}

							if (ImGui::MenuItem("Sphere")) {
					auto entity = m_Context->CreateEntity("Sphere");
					if (auto* data = m_Context->GetEntityData(entity.GetID())) {
						data->Mesh = new MeshComponent();
						data->Mesh->mesh = StandardMeshManager::Instance().GetSphereMesh();
						data->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();
						data->Mesh->MeshName = "Sphere";
					}
					*m_SelectedEntity = entity.GetID();
				}

                if (ImGui::MenuItem("Terrain")) {
                    auto entity = m_Context->CreateEntity("Terrain");
                    if (auto* data = m_Context->GetEntityData(entity.GetID())) {
                        data->Terrain = new TerrainComponent();
                        // Center terrain by offsetting transform so that terrain lies around origin
                        data->Transform.Position = glm::vec3(-0.5f * data->Terrain->Size, 0.0f, -0.5f * data->Terrain->Size);
                    }
                    *m_SelectedEntity = entity.GetID();
                }

                if (ImGui::MenuItem("Particle Emitter")) {
                    auto entity = m_Context->CreateEntity("Particle Emitter");
                    if (auto* data = m_Context->GetEntityData(entity.GetID())) {
                        data->Emitter = new ParticleEmitterComponent();
                    }
                    *m_SelectedEntity = entity.GetID();
                }

            if (ImGui::BeginMenu("Light")) {
                if (ImGui::MenuItem("Directional")) {
                    auto entity = m_Context->CreateEntity("Directional Light");
                    if (auto* data = m_Context->GetEntityData(entity.GetID())) {
                        data->Light = new LightComponent{ LightType::Directional, {1.0f, 1.0f, 1.0f}, 1.0f };
                    }
                    *m_SelectedEntity = entity.GetID();
                }
                if (ImGui::MenuItem("Point")) {
                    auto entity = m_Context->CreateEntity("Point Light");
                    if (auto* data = m_Context->GetEntityData(entity.GetID())) {
                        data->Light = new LightComponent{ LightType::Point, {1.0f, 1.0f, 1.0f}, 1.0f };
                    }
                    *m_SelectedEntity = entity.GetID();
                }
                ImGui::EndMenu(); // Light submenu
            }
            ImGui::EndMenu(); // Create submenu
        }
        ImGui::EndMenu();
    }
}

