#define GLFW_EXPOSE_NATIVE_WIN32
#define NOMINMAX

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include "pipeline/AssetRegistry.h"
#include "pipeline/AssetMetadata.h"
#include <algorithm>
#include <fstream>

#include "Application.h"
#include "rendering/Renderer.h"
#include "rendering/ShaderManager.h"
#include "imgui_impl_bgfx_docking.h"
#include "backends/imgui_impl_glfw.h"
#include <imgui.h>
#include <editor/Input.h>
#include <rendering/Picking.h>
#include "ecs/SkinningSystem.h"
#include <utils/Time.h>
#include <scripting/DotNetHost.h>
#include "editor/Project.h"
#include "physics/Physics.h"
#include "io/FileSystem.h"
#include "serialization/Serializer.h"
#include "pipeline/AssetPipeline.h"
#include "pipeline/AssetLibrary.h"
#include "utils/Profiler.h"
// Application.cpp
Application* Application::s_Instance = nullptr;


// =============================================================
// CONSTRUCTOR / INITIALIZATION
// =============================================================
Application::Application(int width, int height, const std::string& title)
    : m_width(width), m_height(height)
{

    if (s_Instance)
        throw std::runtime_error("Only one instance of Application is allowed!");

    s_Instance = this;

    // 1. Resolve project path relative to executable (usually .../claymore/out/build)
    std::filesystem::path projPath = std::filesystem::current_path();
    std::filesystem::path rawProjPath = projPath / "../../../ClayProject";

    // 2. Canonicalize it safely
    std::filesystem::path defaultProjPath = std::filesystem::weakly_canonical(rawProjPath);

	Project::SetProjectDirectory(defaultProjPath);

    // 3. Verify it exists
    if (!std::filesystem::exists(defaultProjPath)) {
        std::cerr << "[Init] Project directory does not exist: " << defaultProjPath << std::endl;
        // Optional: std::filesystem::create_directories(defaultProjPath);
    }

    // Editor mode: register GUID→path for assets so GUID references resolve when swapping scenes
    {
        std::filesystem::path assetsDir = Project::GetProjectDirectory() / "assets";
        if (std::filesystem::exists(assetsDir)) {
            for (auto& entry : std::filesystem::recursive_directory_iterator(assetsDir)) {
                if (!entry.is_regular_file()) continue;
                std::string abs = entry.path().string();
                const AssetMetadata* meta = AssetRegistry::Instance().GetMetadata(abs);
                AssetMetadata metaLocal;
                if (!meta) {
                    // Try read sidecar .meta file
                    std::filesystem::path metaPath = entry.path(); metaPath += ".meta";
                    if (std::filesystem::exists(metaPath)) {
                        try {
                            std::ifstream mi(metaPath.string());
                            nlohmann::json mj; mi >> mj; mi.close();
                            metaLocal = mj.get<AssetMetadata>();
                            meta = &metaLocal;
                        } catch(...) {}
                    }
                }
                if (!meta) continue;
                if (meta->guid == ClaymoreGUID()) continue;
                // Normalize to virtual path (assets/..)
                std::string vpath = abs;
                std::replace(vpath.begin(), vpath.end(), '\\', '/');
                auto pos = vpath.find("assets/");
                if (pos != std::string::npos) vpath = vpath.substr(pos);
                // Infer type from extension
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                AssetType at = AssetType::Mesh;
                if (ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".obj") at = AssetType::Mesh;
                else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") at = AssetType::Texture;
                else if (ext == ".prefab") at = AssetType::Prefab;
                AssetLibrary::Instance().RegisterAsset(AssetReference(meta->guid, 0, static_cast<int32_t>(at)), at, vpath, entry.path().filename().string());
                // Also map absolute path so serializers that wrote absolute paths can resolve
                AssetLibrary::Instance().RegisterPathAlias(meta->guid, abs);
            }
        }
    }

    // Try mounting a pak next to the executable for standalone mode (DO THIS EARLY)
    {
        std::filesystem::path exeDir = std::filesystem::current_path();
        bool mounted = false;
        // 1) Look for any .pak file in the directory
        for (auto& entry : std::filesystem::directory_iterator(exeDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".pak") {
                if (FileSystem::Instance().MountPak(entry.path().string())) {
                    mounted = true; break;
                }
            }
        }
        // 2) Fallback names if none found by scan
        if (!mounted) {
            std::filesystem::path pakA = exeDir / (Project::GetProjectName() + ".pak");
            std::filesystem::path pakB = exeDir / "Game.pak";
            if (std::filesystem::exists(pakA)) {
                FileSystem::Instance().MountPak(pakA.string());
            } else if (std::filesystem::exists(pakB)) {
                FileSystem::Instance().MountPak(pakB.string());
            }
        }
    }

    // Decide runtime mode BEFORE initializing renderer so we can gate editor-only work
    // Force play-mode if export marker exists next to the executable
    bool forceGameMode = std::filesystem::exists(std::filesystem::current_path() / "game_mode_only.marker");
    m_RunEditorUI = !(FileSystem::Instance().IsPakMounted() || forceGameMode);

    // 1. Initialize Window (GLFW)
    InitWindow(width, height, title);

    // 2. Initialize BGFX Renderer (shader compile gated by m_RunEditorUI)
    InitBgfx();
    // Standalone/game mode: render directly to backbuffer instead of editor offscreen target
    if (!m_RunEditorUI) {
        Renderer::Get().SetRenderToOffscreen(false);
    }

    // 3. Initialize ImGui (editor mode only)
    if (m_RunEditorUI) {
        InitImGui();
    }

    // 4. Initialize Physics System
    Physics::Get().Init();

    // 5. Input Init
    Input::Init(m_window);

    unsigned hw = std::thread::hardware_concurrency();
    size_t workers = (hw > 2) ? (hw - 1) : 1;
    m_Jobs = std::make_unique<JobSystem>(workers);

    // Init Dotnet
    std::filesystem::path fullPath = std::filesystem::current_path() / "ClaymoreEngine.dll";
    LoadDotnetRuntime(
       L"ClaymoreEngine.dll", // Relative or full path works due to absolute() above
       L"ClaymoreEngine.EngineEntry, ClaymoreEngine",
       L"ManagedStart"
    );
    
    // 4. Initialize Asset Pipeline + Watcher (editor only)
    m_AssetPipeline = std::make_unique<AssetPipeline>();
    m_AssetWatcher = std::make_unique<AssetWatcher>(*m_AssetPipeline, defaultProjPath.string());
    if (m_RunEditorUI) {
        m_AssetWatcher->Start();
		uiLayer->LoadProject(defaultProjPath.string());
		Scene::CurrentScene = &uiLayer->GetScene();
    } else {
        m_GameScene = std::make_unique<Scene>();
        Scene::CurrentScene = m_GameScene.get();
    }

    // If running with a mounted pak, try to load entry scene from manifest
    if (FileSystem::Instance().IsPakMounted()) {
        std::string manifestText;
        if (FileSystem::Instance().ReadTextFile("game_manifest.json", manifestText)) {
            try {
                nlohmann::json j = nlohmann::json::parse(manifestText);
                // Load asset GUID->path map first so scene deserialization can resolve meshes/materials
                if (j.contains("assetMap") && j["assetMap"].is_array()) {
                    for (auto& rec : j["assetMap"]) {
                        std::string guidStr = rec.value("guid", "");
                        std::string vpath   = rec.value("path", "");
                        if (!guidStr.empty() && !vpath.empty()) {
                            ClaymoreGUID g = ClaymoreGUID::FromString(guidStr);
                            // Register into AssetLibrary for runtime lookup
                            AssetReference aref(g);
                            AssetLibrary::Instance().RegisterAsset(aref, AssetType::Mesh, vpath, vpath);
                        }
                    }
                }
                std::string entry = j.value("entryScene", "");
                if (!entry.empty()) {
                    Serializer::LoadSceneFromFile(entry, *Scene::CurrentScene);
                    if (m_RunEditorUI) uiLayer->SetCurrentScenePath(entry);
                }
            } catch(const std::exception& e) {
                std::cerr << "[Init] Failed parsing game_manifest.json: " << e.what() << std::endl;
            }
        }
    }

    // In exported/game mode (no editor UI), create a runtime clone and enter play
    if (!m_RunEditorUI) {
        if (m_GameScene) {
            m_RuntimeScene = m_GameScene->RuntimeClone();
            if (m_RuntimeScene) {
                m_RuntimeScene->m_IsPlaying = true;
                Scene::CurrentScene = m_RuntimeScene.get();
                // Debug: report entering play mode and script counts
                size_t scriptCount = 0;
                for (const auto& e : m_RuntimeScene->GetEntities()) {
                    auto* d = m_RuntimeScene->GetEntityData(e.GetID());
                    if (d) scriptCount += d->Scripts.size();
                }
                std::cout << "[Game] Entered play mode (runtime clone). Scripts attached: " << scriptCount << std::endl;
            }
        }
    }

    std::cout << "[Application] Initialization complete." << std::endl;
}

Application::~Application() {
    Shutdown();
}

// =============================================================
// WINDOW SETUP
// =============================================================
void Application::InitWindow(int width, int height, const std::string& title) {
    std::cout << "[Application] Initializing window: " << width << "x" << height
        << " Title: " << title << std::endl;

    if (!glfwInit()) {
        throw std::runtime_error("[Application] Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL context
    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        throw std::runtime_error("[Application] Failed to create GLFW window");
    }

    /*Input::Init(m_window);*/
    std::cout << "[Application] GLFW window created successfully." << std::endl;

    // Handle window resize: update app dimensions, bgfx reset, and renderer framebuffer
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* win, int w, int h){
        if (w <= 0 || h <= 0) return;
        Application& app = Application::Get();
        app.m_width = w;
        app.m_height = h;
        // Reset bgfx backbuffer to new size
        bgfx::reset((uint32_t)w, (uint32_t)h, BGFX_RESET_VSYNC);
        // Notify renderer to resize its offscreen scene texture/framebuffer
        Renderer::Get().Resize((uint32_t)w, (uint32_t)h);
    });
} 

// ============================================================= 
// BGFX SETUP
// =============================================================
void Application::InitBgfx() {
    std::cout << "[Application] Initializing bgfx..." << std::endl;
    // Only compile shaders in editor mode; standalone relies on precompiled .bin files
    if (m_RunEditorUI) {
        ShaderManager::Instance().CompileAllShaders();
    }

    Renderer::Get().Init(m_width, m_height, glfwGetWin32Window(m_window));
    std::cout << "[Application] bgfx initialized." << std::endl;
}

// =============================================================
// IMGUI SETUP
// =============================================================
void Application::InitImGui() { 
    std::cout << "[Application] Initializing ImGui..." << std::endl;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable; // Disabled for now
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigWindowsResizeFromEdges = true;

    // Style
    ImGui::StyleColorsDark();

    // Font (DPI aware)
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(m_window, &xscale, &yscale);
    float contentScale = std::max(xscale, yscale);
    float baseFontSize = 16.0f * contentScale;
    io.Fonts->Clear();
    ImFontConfig cfg;
    cfg.OversampleH = 3;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = false;
    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", baseFontSize, &cfg);
    io.Fonts->Build();
    io.FontGlobalScale = 1.0f; // fonts already scaled above

    // Backend Init
    ImGui_ImplGlfw_InitForOther(m_window, true);
    ImGui_ImplBgfx_Init(255);

    // Editor UI Layer
    uiLayer = std::make_unique<UILayer>();
    uiLayer->ApplyStyle();

    std::cout << "[Application] ImGui initialized." << std::endl;
}

// =============================================================
// MAIN LOOP
// =============================================================
void Application::Run() {
    std::cout << "[Application] Running main loop..." << std::endl;
    Time::Init();

    // In your app/game-loop bootstrap, ON THE THREAD that will call Scene::Update: 
    if (InstallSyncContextPtr) {
       InstallSyncContextPtr();
       }

    while (!glfwWindowShouldClose(m_window)) {
        Profiler::Get().BeginFrame();
        ScopedTimer frameTimer("Frame");
        glfwPollEvents();
        Time::Tick();

        float dt = Time::GetDeltaTime();
        // Scene reference must be determined AFTER UI may toggle play/stop
        // Do not bind a reference to *Scene::CurrentScene here.
        // --------------------------------------
        // ASSET PIPELINE PROCESSING
        // --------------------------------------
        // Handles:
        // 1. Files flagged by AssetWatcher → Queued imports
        // 2. CPU pre-processing (decoding textures/models)
        // 3. GPU uploads (executed on main thread for safety)
        m_AssetPipeline->ProcessMainThreadTasks();

        // --------------------------------------
        // INPUT UPDATE
        // --------------------------------------
        Input::Update();

        // --------------------------------------
        // START NEW IMGUI FRAME (editor mode only)
        // --------------------------------------
        if (m_RunEditorUI) {
            ImGui_ImplGlfw_NewFrame();
            ImGui_ImplBgfx_NewFrame();
            ImGui::NewFrame();
        }

        // --------------------------------------
        // UI RENDER (Docking Panels, Inspector, etc.) (editor mode only)
        // This may toggle Play/Stop and create/destroy the runtime clone.
        // --------------------------------------
        if (m_RunEditorUI) {
            /*uiLayer->HandleCameraControls();*/
            {
                ScopedTimer t("UI");
                uiLayer->OnUIRender();
            }
        }

        // --------------------------------------
        // SCENE UPDATE (decide after UI may have toggled Play/Stop)
        // --------------------------------------
        if (m_RunEditorUI) {
            Scene& editorScene = uiLayer->GetScene();
            if (editorScene.m_RuntimeScene) {
                // Ensure CurrentScene points to runtime clone
                if (Scene::CurrentScene != editorScene.m_RuntimeScene.get()) {
                    Scene::CurrentScene = editorScene.m_RuntimeScene.get();
                    if (InstallSyncContextPtr) InstallSyncContextPtr();
                    if (ClearSyncContextPtr) ClearSyncContextPtr();
                }
                {
                    ScopedTimer t("Scene/Update (Play)");
                    editorScene.m_RuntimeScene->Update(dt);
                }
            } else {
                // Ensure CurrentScene points back to editor scene
                if (Scene::CurrentScene != &editorScene) {
                    Scene::CurrentScene = &editorScene;
                    if (ClearSyncContextPtr) ClearSyncContextPtr();
                }
                {
                    ScopedTimer t("Scene/Update (Edit)");
                    editorScene.Update(dt);
                }
                {
                    ScopedTimer t("Skinning");
                    SkinningSystem::Update(editorScene);
                }
            }
        } else {
            // Game mode without editor UI
            if (Scene::CurrentScene) {
                ScopedTimer t("Scene/Update (Game)");
                Scene::CurrentScene->Update(dt);
            }
        }

        // --------------------------------------
        // SCENE RENDER
        // --------------------------------------
        {
            ScopedTimer t("Renderer/BeginFrame");
            Renderer::Get().BeginFrame(0.1f, 0.1f, 0.1f);
        }
        if (m_RunEditorUI) {
            Scene& editorScene = uiLayer->GetScene();
            if (editorScene.m_RuntimeScene) {
                ScopedTimer t("Renderer/RenderScene (Play)");
                Renderer::Get().RenderScene(*editorScene.m_RuntimeScene);
            } else {
                {
                    ScopedTimer t("Renderer/RenderScene (Edit)");
                    Renderer::Get().RenderScene(editorScene);
                }
                // Editor-only: draw outline for selected entity
                {
                    ScopedTimer t("Renderer/DrawOutline");
                    Renderer::Get().DrawEntityOutline(editorScene, uiLayer->GetSelectedEntity());
                }
            }
        } else {
            ScopedTimer t("Renderer/RenderScene (Game)");
            Renderer::Get().RenderScene(*Scene::CurrentScene);
        }

        // --------------------------------------
        // ENTITY PICKING (skip if UI consumed input this frame) (editor mode only)
        // --------------------------------------
        if (m_RunEditorUI && !Renderer::Get().WasUIInputConsumedThisFrame()) {
            ScopedTimer t("Picking");
            Picking::Process(uiLayer->GetScene(), Renderer::Get().GetCamera());
        }
        if (m_RunEditorUI) {
            int pickedEntity = Picking::GetLastPick();
            if (pickedEntity != -1) {
                std::cout << "[Debug] Picked Entity: " << pickedEntity << std::endl;
                uiLayer->SetSelectedEntity(pickedEntity);
                // Clear any legacy timeline selection (new panel manages its own inspector)
            }
        }

        // --------------------------------------
        // IMGUI RENDER PASS (editor mode only)
        // --------------------------------------
        if (m_RunEditorUI) {
            ScopedTimer t("UI/Render");
            ImGui::Render();
            bgfx::setViewFrameBuffer(255, BGFX_INVALID_HANDLE);
            bgfx::setViewRect(255, 0, 0, uint16_t(m_width), uint16_t(m_height));
            bgfx::touch(255);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
            ImGui_ImplBgfx_Render(255, ImGui::GetDrawData(), 0x00000000);
        }

        // --------------------------------------
        // SUBMIT FRAME
        // --------------------------------------
        {
            ScopedTimer t("Renderer/SubmitFrame");
            (void)bgfx::frame();
        }
        Profiler::Get().EndFrame();

    }

    std::cout << "[Application] Main loop ended." << std::endl;
}

// =============================================================
// SHUTDOWN
// =============================================================
void Application::Shutdown() {
    std::cout << "[Application] Shutting down..." << std::endl;

    // Shutdown Physics System
    Physics::Get().Shutdown();

    if (m_AssetWatcher) {
        m_AssetWatcher->Stop();
    }

    m_Jobs.reset();

    if (m_RunEditorUI) {
        ImGui_ImplBgfx_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    glfwDestroyWindow(m_window);
    glfwTerminate();

    std::cout << "[Application] Shutdown complete." << std::endl;
}

Application& Application::Get() {
    if (!s_Instance)
        throw std::runtime_error("Application::Get() called before Application was created!");
    return *s_Instance;
}

// ------------------------------------------------------------
// Playmode controls (editor mode only)
// ------------------------------------------------------------
void Application::StartPlayMode() {
    if (m_IsPlaying) return;
    if (!m_RunEditorUI) return;
    if (!uiLayer) return;

    Scene& editorScene = uiLayer->GetScene();
    if (editorScene.m_RuntimeScene) return;

    editorScene.m_RuntimeScene = editorScene.RuntimeClone();
    if (editorScene.m_RuntimeScene) {
        editorScene.m_RuntimeScene->m_IsPlaying = true;
        Scene::CurrentScene = editorScene.m_RuntimeScene.get();
        m_IsPlaying = true;
    }
}

void Application::StopPlayMode() {
    if (!m_IsPlaying) return;
    if (!m_RunEditorUI) return;
    if (!uiLayer) return;

    Scene& editorScene = uiLayer->GetScene();
    editorScene.m_RuntimeScene.reset();
    Scene::CurrentScene = &editorScene;
    m_IsPlaying = false;
}