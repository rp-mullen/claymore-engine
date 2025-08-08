#define GLFW_EXPOSE_NATIVE_WIN32
#define NOMINMAX

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#include <iostream>
#include <algorithm>

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


    // 1. Initialize Window (GLFW)
    InitWindow(width, height, title);

    // 2. Initialize BGFX Renderer
    InitBgfx();

    // 3. Initialize ImGui
    InitImGui();

    // 4. Initialize Physics System
    Physics::Get().Init();

    // 5. Input Init
    Input::Init(m_window);

    // Init Dotnet
    std::filesystem::path fullPath = std::filesystem::current_path() / "ClaymoreEngine.dll";
    LoadDotnetRuntime(
       L"ClaymoreEngine.dll", // Relative or full path works due to absolute() above
       L"ClaymoreEngine.EngineEntry, ClaymoreEngine",
       L"ManagedStart"
    );
    
    // 4. Initialize Asset Pipeline + Watcher
    m_AssetPipeline = std::make_unique<AssetPipeline>();
    m_AssetWatcher = std::make_unique<AssetWatcher>(*m_AssetPipeline, defaultProjPath.string());
    m_AssetWatcher->Start();

	uiLayer->LoadProject(defaultProjPath.string());
	Scene::CurrentScene = &uiLayer->GetScene();

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
    // Compile shaders on startup before Renderer tries to load programs
    ShaderManager::Instance().CompileAllShaders();

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
        glfwPollEvents();
        Time::Tick();

        float dt = Time::GetDeltaTime();
        auto scene = uiLayer->GetScene();
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
        // START NEW IMGUI FRAME
        // --------------------------------------
        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplBgfx_NewFrame();
        ImGui::NewFrame();

        // --------------------------------------
        // UI RENDER (Docking Panels, Inspector, etc.)
        // --------------------------------------
        /*uiLayer->HandleCameraControls();*/
        uiLayer->OnUIRender();

        // --------------------------------------
        // SCENE UPDATE
        // --------------------------------------
        if (scene.m_RuntimeScene) {

			if (Scene::CurrentScene != scene.m_RuntimeScene.get()) {
				Scene::CurrentScene = scene.m_RuntimeScene.get();
            if (InstallSyncContextPtr) InstallSyncContextPtr();
			}

           scene.m_RuntimeScene->Update(dt);
           }
        else {
           scene.Update(dt);
            SkinningSystem::Update(scene);
           }

        // --------------------------------------
        // SCENE RENDER
        // --------------------------------------
        Renderer::Get().BeginFrame(0.1f, 0.1f, 0.1f);
        if (scene.m_RuntimeScene) {
           Renderer::Get().RenderScene(*scene.m_RuntimeScene);
           }
        else {
           Renderer::Get().RenderScene(scene);
           }

        // --------------------------------------
        // ENTITY PICKING
        // --------------------------------------
        Picking::Process(uiLayer->GetScene(), Renderer::Get().GetCamera());
        int pickedEntity = Picking::GetLastPick();
        if (pickedEntity != -1) {
            std::cout << "[Debug] Picked Entity: " << pickedEntity << std::endl;
            uiLayer->SetSelectedEntity(pickedEntity);
        }

        // --------------------------------------
        // IMGUI RENDER PASS
        // --------------------------------------
        ImGui::Render();
        bgfx::setViewFrameBuffer(255, BGFX_INVALID_HANDLE);
        bgfx::setViewRect(255, 0, 0, uint16_t(m_width), uint16_t(m_height));
        bgfx::touch(255);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        ImGui_ImplBgfx_Render(255, ImGui::GetDrawData(), 0x00000000);

        // --------------------------------------
        // SUBMIT FRAME
        // --------------------------------------
        uint32_t frameNum = bgfx::frame();

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

    ImGui_ImplBgfx_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(m_window);
    glfwTerminate();

    std::cout << "[Application] Shutdown complete." << std::endl;
}

Application& Application::Get() {
    if (!s_Instance)
        throw std::runtime_error("Application::Get() called before Application was created!");
    return *s_Instance;
}
