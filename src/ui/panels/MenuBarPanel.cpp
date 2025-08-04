#include "MenuBarPanel.h"
#include "ecs/Entity.h"
#include "ecs/Components.h"
#include <editor/Project.h>
#include "serialization/Serializer.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>  // JSON serialization
#include <rendering/StandardMeshManager.h>
#include <rendering/MaterialManager.h>

using json = nlohmann::json;

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
            return std::string(ws.begin(), ws.end());
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
        
        if (ImGui::MenuItem("Save Scene As...")) {
            // TODO: Implement file dialog for saving
            std::string scenePath = "assets/scenes/CurrentScene.scene";
            if (Serializer::SaveSceneToFile(*m_Context, scenePath)) {
                std::cout << "Scene saved successfully!" << std::endl;
            }
        }
        
        if (ImGui::MenuItem("Load Scene...")) {
            // TODO: Implement file dialog for loading
            std::string scenePath = "assets/scenes/SampleScene.scene";
            if (Serializer::LoadSceneFromFile(scenePath, *m_Context)) {
                std::cout << "Scene loaded successfully!" << std::endl;
                *m_SelectedEntity = -1; // Clear selection
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

    // ENTITY MENU
    if (ImGui::BeginMenu("Entity")) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Empty")) {
                auto entity = m_Context->CreateEntity("Empty Entity");
                *m_SelectedEntity = entity.GetID();
            }

			if (ImGui::MenuItem("Cube")) {
				auto entity = m_Context->CreateEntity("Cube");
				if (auto* data = m_Context->GetEntityData(entity.GetID())) {
					data->Mesh = new MeshComponent();
					data->Mesh->mesh = StandardMeshManager::Instance().GetCubeMesh();
					data->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();
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
