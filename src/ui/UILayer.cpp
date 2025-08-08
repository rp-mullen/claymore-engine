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
    m_LayoutInitialized = false;
    RegisterComponentDrawers();
    RegisterSampleScriptProperties();

    // Register primitive meshes with AssetLibrary
    StandardMeshManager::Instance().RegisterPrimitiveMeshes();

    CreateDebugCubeEntity();
    CreateDefaultLight();
}
void UILayer::RequestLayoutReset() {
    m_ResetLayoutRequested = true;
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

   // Base colors (dark slate + accent blue)
   colors[ImGuiCol_WindowBg]        = ImVec4(0.13f, 0.13f, 0.14f, 1.00f);
   colors[ImGuiCol_ChildBg]         = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
   colors[ImGuiCol_PopupBg]         = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
   colors[ImGuiCol_Border]          = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
   colors[ImGuiCol_BorderShadow]    = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
   colors[ImGuiCol_Text]            = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);

   // Headers (Collapsing, Menus)
   colors[ImGuiCol_Header]          = ImVec4(0.20f, 0.22f, 0.25f, 0.80f);
   colors[ImGuiCol_HeaderHovered]   = ImVec4(0.26f, 0.29f, 0.33f, 0.90f);
   colors[ImGuiCol_HeaderActive]    = ImVec4(0.28f, 0.31f, 0.36f, 1.00f);

   // Buttons
   colors[ImGuiCol_Button]          = ImVec4(0.22f, 0.24f, 0.28f, 0.85f);
   colors[ImGuiCol_ButtonHovered]   = ImVec4(0.28f, 0.55f, 0.92f, 0.90f);
   colors[ImGuiCol_ButtonActive]    = ImVec4(0.20f, 0.48f, 0.86f, 1.00f);

   // Frame BG
   colors[ImGuiCol_FrameBg]         = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
   colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.28f, 0.55f, 0.92f, 0.40f);
   colors[ImGuiCol_FrameBgActive]   = ImVec4(0.28f, 0.55f, 0.92f, 0.67f);

   // Tabs
   colors[ImGuiCol_Tab]             = ImVec4(0.11f, 0.12f, 0.13f, 1.00f);
   colors[ImGuiCol_TabHovered]      = ImVec4(0.28f, 0.55f, 0.92f, 0.80f);
   colors[ImGuiCol_TabActive]       = ImVec4(0.18f, 0.19f, 0.20f, 1.00f);
   colors[ImGuiCol_TabUnfocused]    = ImVec4(0.11f, 0.12f, 0.13f, 1.00f);
   colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.19f, 0.20f, 1.00f);

   // Title bar
   colors[ImGuiCol_TitleBg]         = ImVec4(0.09f, 0.10f, 0.11f, 1.00f);
   colors[ImGuiCol_TitleBgActive]   = ImVec4(0.12f, 0.13f, 0.14f, 1.00f);
   colors[ImGuiCol_TitleBgCollapsed]= ImVec4(0.09f, 0.10f, 0.11f, 0.75f);

   // Scrollbars
   colors[ImGuiCol_ScrollbarBg]     = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
   colors[ImGuiCol_ScrollbarGrab]   = ImVec4(0.24f, 0.25f, 0.27f, 1.00f);
   colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.29f, 0.31f, 1.00f);
   colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.34f, 0.35f, 0.38f, 1.00f);

   // Resize grip
   colors[ImGuiCol_ResizeGrip]      = ImVec4(0.28f, 0.55f, 0.92f, 0.25f);
   colors[ImGuiCol_ResizeGripHovered]= ImVec4(0.28f, 0.55f, 0.92f, 0.67f);
   colors[ImGuiCol_ResizeGripActive]= ImVec4(0.28f, 0.55f, 0.92f, 1.00f);

   // Check/Slider
   colors[ImGuiCol_CheckMark]       = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
   colors[ImGuiCol_SliderGrab]      = ImVec4(0.28f, 0.55f, 0.92f, 0.80f);
   colors[ImGuiCol_SliderGrabActive]= ImVec4(0.28f, 0.55f, 0.92f, 1.00f);

   // Styling tweaks
   style.WindowRounding   = 6.0f;
   style.FrameRounding    = 4.0f;
   style.ScrollbarRounding= 6.0f;
   style.GrabRounding     = 4.0f;
   style.TabRounding      = 4.0f;
   style.WindowPadding    = ImVec2(8, 8);
   style.FramePadding     = ImVec2(8, 6);
   style.ItemSpacing      = ImVec2(8, 6);
}
// =============================
// Main UI Render Loop
// =============================
void UILayer::OnUIRender() {
    BeginDockspace();

    // Determine which scene should be considered "active" for editor panels
    Scene* activeScene = m_PlayMode && m_Scene.m_RuntimeScene ? m_Scene.m_RuntimeScene.get() : &m_Scene;

    // Update panel contexts based on whether any prefab editors are open
    if (!m_PrefabEditors.empty()) {
        // Detach core panels from the active scene while editing a prefab
        // Save current selection so it can be restored when returning
        m_PreviousSelectedEntity = m_SelectedEntity;

        m_SceneHierarchyPanel.SetContext(nullptr);
        m_InspectorPanel.SetContext(nullptr);
        m_ViewportPanel.SetContext(nullptr);
        m_SelectedEntity = -1;
    } else {
        // Re-attach when no prefab editors are open (honour play/edit mode)
        m_SceneHierarchyPanel.SetContext(activeScene);
        m_InspectorPanel.SetContext(activeScene);
        m_ViewportPanel.SetContext(activeScene);

        // Restore previous entity selection if any
        if (m_PreviousSelectedEntity != -1)
            m_SelectedEntity = m_PreviousSelectedEntity;
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
    // Force this host window to be undocked to remain a root window for DockSpace
    ImGui::SetNextWindowDockID(0, ImGuiCond_Always);

    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    // Optional: pass-through central node look
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    // DockSpace main area
    m_MainDockspaceID = ImGui::GetID("MyDockSpace");
    ImGuiID dockspace_id = m_MainDockspaceID;

    // Ensure the dockspace node is a root node; if corrupted (e.g., created inside a child), rebuild it
    if (ImGuiDockNode* existingNode = ImGui::DockBuilderGetNode(m_MainDockspaceID)) {
        if (existingNode->ParentNode != nullptr) {
            ImGui::DockBuilderRemoveNode(m_MainDockspaceID);
            m_LayoutInitialized = false;
        }
    }

    // Default professional layout on first run or when reset requested
    if (!m_LayoutInitialized || m_ResetLayoutRequested) {
        m_ResetLayoutRequested = false;
        m_LayoutInitialized = true;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        ImGuiID dock_left, dock_right, dock_down;
        dock_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.22f, nullptr, &dockspace_id);
        dock_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.28f, nullptr, &dockspace_id);
        dock_down = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.26f, nullptr, &dockspace_id);

        ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Project", dock_down);
        ImGui::DockBuilderDockWindow("Console", dock_down);
        ImGui::DockBuilderDockWindow("Script Registry", dock_right);
        ImGui::DockBuilderDockWindow("Viewport", dockspace_id);
        ImGui::DockBuilderFinish(m_MainDockspaceID);
    }

    // Menu Bar
    if (ImGui::BeginMenuBar()) {
        // Branding (left)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.55f, 0.92f, 1.0f));
        ImGui::Text("Claymore");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
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

    // Colorful Play/Stop button
    bool playMode = m_ToolbarPanel.IsPlayMode();
    if (playMode) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.20f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.30f, 0.34f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.65f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.72f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.78f, 0.45f, 1.0f));
    }
    if (ImGui::Button(playMode ? "Stop" : "Play", ImVec2(buttonWidth, buttonHeight)))
        m_ToolbarPanel.TogglePlayMode();
    ImGui::PopStyleColor(3);

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // DockSpace (below toolbar), reserve space for status bar using negative height
    ImGui::Separator();
    const float statusBarHeight = ImGui::GetFrameHeight();
    ImGuiID rootDockspaceId = m_MainDockspaceID;
    if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(m_MainDockspaceID)) {
        while (node->ParentNode != nullptr) node = node->ParentNode;
        rootDockspaceId = node->ID;
    }
    ImGui::DockSpace(rootDockspaceId, ImVec2(0.0f, -statusBarHeight), ImGuiDockNodeFlags_None);

    // Status bar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
    ImGui::BeginChild("StatusBar", ImVec2(0, statusBarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::TextDisabled("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::SameLine();
    ImGui::TextDisabled("Entities: %d", (int)m_Scene.GetEntities().size());
    ImGui::SameLine();
    ImGui::TextDisabled("| Mode: %s", m_PlayMode ? "Play" : "Edit");
    ImGui::SameLine();
    const char* selName = "None";
    if (m_SelectedEntity != (EntityID)-1) {
        if (auto* data = m_Scene.GetEntityData(m_SelectedEntity)) selName = data->Name.c_str();
    }
    ImGui::TextDisabled("| Selected: %s", selName);
    ImGui::SameLine();
    if (!m_ProjectPanel.GetSelectedItemName().empty()) {
        ImGui::TextDisabled("| File: %s", m_ProjectPanel.GetSelectedItemName().c_str());
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

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
