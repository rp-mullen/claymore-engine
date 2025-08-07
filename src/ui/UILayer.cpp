#include "UILayer.h"
#include <imgui_internal.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include "rendering/Renderer.h"
#include "rendering/ModelLoader.h"
#include <bx/math.h>
#include <rendering/ShaderManager.h>
#include <rendering/TextureLoader.h>
#include "panels/InspectorPanel.h"
#include "Logger.h"
#include <rendering/MaterialManager.h>
#include <rendering/StandardMeshManager.h>
#include "utility/ComponentDrawerRegistry.h"
#include "scripting/ScriptReflectionSetup.h"
#include "utility/ComponentDrawerSetup.h"
#include "utils/TerrainPainter.h"
#include <ecs/debug/TestScript.h>
#include <glm/glm.hpp>
#include <core/application.h>
#include "ecs/EntityData.h"
#include "ecs/Components.h"
#include <memory>

namespace fs = std::filesystem;
std::vector<std::string> g_RegisteredScriptNames;

// =============================
// Constructor / Initialization
// =============================
UILayer::UILayer()
    : m_InspectorPanel(&m_Scene, &m_SelectedEntity),
      m_ProjectPanel(&m_Scene, this),
      m_ViewportPanel(m_Scene, &m_SelectedEntity),
      m_SceneHierarchyPanel(&m_Scene, &m_SelectedEntity),
      m_MenuBarPanel(&m_Scene, &m_SelectedEntity, &m_ProjectPanel, this)
{
    m_ToolbarPanel = ToolbarPanel(this);

    Logger::SetCallback([this](const std::string& msg, LogLevel level) {
        m_ConsolePanel.AddLog(msg, level);
        if (level == LogLevel::Error)
            m_FocusConsoleNextFrame = true;
    });

    ApplyStyle();
    RegisterComponentDrawers();
    RegisterSampleScriptProperties();

    // Register primitive meshes with AssetLibrary
    StandardMeshManager::Instance().RegisterPrimitiveMeshes();

    CreateDebugCubeEntity();
    CreateDefaultLight();
}

void UILayer::LoadProject(std::string path) {
    m_ProjectPanel.LoadProject(path);
    OnAttach();
}

void UILayer::OnAttach() {
    m_ScriptPanel.SetScriptSource(&g_RegisteredScriptNames);
    m_ScriptPanel.SetContext(&m_Scene);
}

// =============================
// UI Style
// =============================
void UILayer::ApplyStyle() {
   ImGuiStyle& style = ImGui::GetStyle();
   ImVec4* colors = style.Colors;

   // Background
   colors[ImGuiCol_WindowBg] = ImVec4(0.219f, 0.219f, 0.219f, 1.00f); // #383838
   colors[ImGuiCol_ChildBg] = ImVec4(0.239f, 0.239f, 0.239f, 1.00f);
   colors[ImGuiCol_PopupBg] = ImVec4(0.239f, 0.239f, 0.239f, 1.00f);
   colors[ImGuiCol_Border] = ImVec4(0.117f, 0.117f, 0.117f, 1.00f);
   colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

   // Headers (Collapsing, Menus)
   colors[ImGuiCol_Header] = ImVec4(0.255f, 0.255f, 0.255f, 0.31f);
   colors[ImGuiCol_HeaderHovered] = ImVec4(0.365f, 0.365f, 0.365f, 0.80f);
   colors[ImGuiCol_HeaderActive] = ImVec4(0.365f, 0.365f, 0.365f, 1.00f);

   // Buttons
   colors[ImGuiCol_Button] = ImVec4(0.255f, 0.255f, 0.255f, 0.31f);
   colors[ImGuiCol_ButtonHovered] = ImVec4(0.365f, 0.365f, 0.365f, 0.80f);
   colors[ImGuiCol_ButtonActive] = ImVec4(0.365f, 0.365f, 0.365f, 1.00f);

   // Frame BG
   colors[ImGuiCol_FrameBg] = ImVec4(0.172f, 0.172f, 0.172f, 1.00f);
   colors[ImGuiCol_FrameBgHovered] = ImVec4(0.255f, 0.255f, 0.255f, 0.31f);
   colors[ImGuiCol_FrameBgActive] = ImVec4(0.365f, 0.365f, 0.365f, 1.00f);

   // Tabs
   colors[ImGuiCol_Tab] = ImVec4(0.219f, 0.219f, 0.219f, 1.00f);
   colors[ImGuiCol_TabHovered] = ImVec4(0.365f, 0.365f, 0.365f, 1.00f);
   colors[ImGuiCol_TabActive] = ImVec4(0.255f, 0.255f, 0.255f, 1.00f);

   // Title
   colors[ImGuiCol_TitleBg] = ImVec4(0.172f, 0.172f, 0.172f, 1.00f);
   colors[ImGuiCol_TitleBgActive] = ImVec4(0.219f, 0.219f, 0.219f, 1.00f);
   colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.172f, 0.172f, 0.172f, 0.75f);

   // Scrollbars
   colors[ImGuiCol_ScrollbarBg] = ImVec4(0.172f, 0.172f, 0.172f, 1.00f);
   colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.365f, 0.365f, 0.365f, 0.80f);
   colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.365f, 0.365f, 0.365f, 1.00f);
   colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.465f, 0.465f, 0.465f, 1.00f);

   // Resize grip
   colors[ImGuiCol_ResizeGrip] = ImVec4(0.255f, 0.255f, 0.255f, 0.31f);
   colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.365f, 0.365f, 0.365f, 0.80f);
   colors[ImGuiCol_ResizeGripActive] = ImVec4(0.465f, 0.465f, 0.465f, 1.00f);

   // Text
   colors[ImGuiCol_Text] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);

   // Styling tweaks
   style.FrameRounding = 6.0f;
   style.WindowRounding = 8.0f;
   style.ScrollbarRounding = 9.0f;
   style.GrabRounding = 4.0f;
   }
// =============================
// Main UI Render Loop
// =============================
void UILayer::OnUIRender() {
    BeginDockspace();

    // Update panel contexts based on whether any prefab editors are open
    if (!m_PrefabEditors.empty()) {
        // Detach core panels from the active scene while editing a prefab
        m_SceneHierarchyPanel.SetContext(nullptr);
        m_InspectorPanel.SetContext(nullptr);
        m_SelectedEntity = -1;
    } else {
        // Re-attach when no prefab editors are open
        m_SceneHierarchyPanel.SetContext(&m_Scene);
        m_InspectorPanel.SetContext(&m_Scene);
    }

    // Core panels
    m_SceneHierarchyPanel.OnImGuiRender();
    m_InspectorPanel.OnImGuiRender();
    m_ProjectPanel.OnImGuiRender();
    m_ConsolePanel.OnImGuiRender();
    if (m_FocusConsoleNextFrame) {
        ImGui::SetWindowFocus("Console");
        m_FocusConsoleNextFrame = false;
    }

    m_ScriptPanel.OnImGuiRender();

    // Main viewport
    m_ViewportPanel.OnImGuiRender(Renderer::Get().GetSceneTexture());

    // Any open prefab editors
    for (auto it = m_PrefabEditors.begin(); it != m_PrefabEditors.end(); ) {
        PrefabEditorPanel* panel = it->get();
        panel->OnImGuiRender();
        if (!panel->IsOpen()) {
            it = m_PrefabEditors.erase(it);
        } else {
            ++it;
        }
    }

    // Editor-only terrain painting
    TerrainPainter::Update(m_Scene, m_SelectedEntity);

    // Deferred scene loading
    ProcessDeferredSceneLoad();
}

// =============================
// Dockspace + Toolbar Layout
// =============================
void UILayer::BeginDockspace() {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    // DockSpace main area
    m_MainDockspaceID = ImGui::GetID("MyDockSpace");
    ImGuiID dockspace_id = m_MainDockspaceID;

    // Menu Bar
    if (ImGui::BeginMenuBar()) {
        m_MenuBarPanel.OnImGuiRender();
        ImGui::EndMenuBar();
    }

    // Toolbar row
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
    ImGui::Separator();
    ImGui::BeginChild("ToolbarRow", ImVec2(0, 40), false, ImGuiWindowFlags_NoScrollbar);

    // Play / Stop centered button
    float buttonWidth = 80.0f;
    float buttonHeight = 30.0f;
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float startX = (availableWidth - buttonWidth) * 0.5f;
    if (startX > 0)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);

    if (ImGui::Button(m_ToolbarPanel.IsPlayMode() ? "Stop" : "Play", ImVec2(buttonWidth, buttonHeight)))
        m_ToolbarPanel.TogglePlayMode();

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // DockSpace (below toolbar)
    ImGui::Separator();
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    ImGui::End();
}

// =============================
// Scene helpers
// =============================
void UILayer::CreateDebugCubeEntity() {
    auto cubeEntity = m_Scene.CreateEntity("Debug Cube");
    EntityData* data = m_Scene.GetEntityData(cubeEntity.GetID());
    data->Mesh = new MeshComponent(
        StandardMeshManager::Instance().GetCubeMesh(),
        std::string("DebugCube"),
        nullptr);
    data->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();
}

void UILayer::CreateDefaultLight() {
    auto lightEntity = m_Scene.CreateEntity("Default Light");
    EntityData* data = m_Scene.GetEntityData(lightEntity.GetID());
    data->Light = new LightComponent(LightType::Directional, glm::vec3(1.0f), 1.0f);
}

// =============================
// Play Mode Toggle
// =============================
void UILayer::TogglePlayMode() {
    m_PlayMode = !m_PlayMode;
    Scene* activeScene = m_PlayMode ? m_Scene.m_RuntimeScene.get() : &m_Scene;

    // Update panel contexts
    m_SceneHierarchyPanel.SetContext(activeScene);
    m_InspectorPanel.SetContext(activeScene);
    m_ViewportPanel.SetContext(activeScene);
}

// =============================
// Prefab Editor Management
// =============================
void UILayer::OpenPrefabEditor(const std::string& prefabPath) {
    m_PrefabEditors.emplace_back(std::make_unique<PrefabEditorPanel>(prefabPath, this));
}

// =============================
// Deferred Scene Loading
// =============================
void UILayer::DeferSceneLoad(const std::string& filepath) {
    m_DeferredScenePath = filepath;
    m_HasDeferredSceneLoad = true;
}

void UILayer::ProcessDeferredSceneLoad() {
    if (!m_HasDeferredSceneLoad) return;

    std::cout << "[UILayer] Processing deferred scene load: " << m_DeferredScenePath << std::endl;
    if (Serializer::LoadSceneFromFile(m_DeferredScenePath, m_Scene)) {
        std::cout << "[UILayer] Successfully loaded scene: " << m_DeferredScenePath << std::endl;
        m_SelectedEntity = -1;
    } else {
        std::cerr << "[UILayer] Failed to load scene: " << m_DeferredScenePath << std::endl;
    }
    m_HasDeferredSceneLoad = false;
    m_DeferredScenePath.clear();
}
