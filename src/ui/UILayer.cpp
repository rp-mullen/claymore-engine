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
#include <ecs/debug/TestScript.h>
#include <core/application.h>
#include "ecs/EntityData.h"

namespace fs = std::filesystem;
std::vector<std::string> g_RegisteredScriptNames;

UILayer::UILayer()
   : m_InspectorPanel(&m_Scene, &m_SelectedEntity),
   m_ProjectPanel(&m_Scene),
   m_ViewportPanel(m_Scene, &m_SelectedEntity),
   m_SceneHierarchyPanel(&m_Scene, &m_SelectedEntity),
   m_MenuBarPanel(&m_Scene, &m_SelectedEntity, &m_ProjectPanel, this) // Pass UILayer reference
   {

   m_ToolbarPanel = ToolbarPanel(this);

    Logger::SetCallback([this](const std::string& msg, LogLevel level) {
        m_ConsolePanel.AddLog(msg, level);
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

void UILayer::OnAttach()
   {
   m_ScriptPanel.SetScriptSource(&g_RegisteredScriptNames);
   m_ScriptPanel.SetContext(&m_Scene);
   }

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

void UILayer::OnUIRender() {

   BeginDockspace();
   m_SceneHierarchyPanel.OnImGuiRender();
   m_InspectorPanel.OnImGuiRender();
   m_ProjectPanel.OnImGuiRender();
   m_ConsolePanel.OnImGuiRender();

   m_ScriptPanel.OnImGuiRender();

   // Pass Renderer’s output texture to viewport
   m_ViewportPanel.OnImGuiRender(Renderer::Get().GetSceneTexture());
   
   // Process deferred scene loading at the end of the frame
   ProcessDeferredSceneLoad();
   }


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
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

    // ✅ Menu Bar
    if (ImGui::BeginMenuBar()) {
        m_MenuBarPanel.OnImGuiRender(); // File, Entity menus
        ImGui::EndMenuBar();
    }

    // ✅ Static Toolbar: Full width below menu bar
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
    // ✅ Static Toolbar: Full width below menu bar
    ImGui::Separator();
    ImGui::BeginChild("ToolbarRow", ImVec2(0, 40), false, ImGuiWindowFlags_NoScrollbar);

    // Calculate total width of buttons + spacing
    float buttonWidth = 80.0f;
    float buttonHeight = 30.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    int buttonCount = 4; // Play, Translate, Rotate, Scale
    float totalWidth = (buttonWidth * buttonCount) + (spacing * (buttonCount - 1));

    // Center alignment
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float startX = (availableWidth - totalWidth) * 0.5f;
    if (startX > 0)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);

    // Buttons
    if (ImGui::Button(m_ToolbarPanel.IsPlayMode() ? "Stop" : "Play", ImVec2(buttonWidth, buttonHeight)))
        m_ToolbarPanel.TogglePlayMode();
    ImGui::SameLine();
    if (ImGui::Button("Translate", ImVec2(buttonWidth, buttonHeight)))
        m_ToolbarPanel.SetOperation(GizmoOperation::Translate);
    ImGui::SameLine();
    if (ImGui::Button("Rotate", ImVec2(buttonWidth, buttonHeight)))
        m_ToolbarPanel.SetOperation(GizmoOperation::Rotate);
    ImGui::SameLine();
    if (ImGui::Button("Scale", ImVec2(buttonWidth, buttonHeight)))
        m_ToolbarPanel.SetOperation(GizmoOperation::Scale);

    ImGui::EndChild();

    ImGui::PopStyleVar();

    // Main DockSpace (below toolbar)
    ImGui::Separator();
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    ImGui::End();
}




 void UILayer::CreateDebugCubeEntity() {

   auto cubeEntity = m_Scene.CreateEntity("Debug Cube");
   EntityData* data = m_Scene.GetEntityData(cubeEntity.GetID());
   data->Mesh = new MeshComponent(
       StandardMeshManager::Instance().GetCubeMesh(),
       std::string("DebugCube"),
       nullptr
   );

   data->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();

   ScriptInstance instance;
   instance.ClassName = "MyTestScript";
   instance.Instance = ScriptSystem::Instance().Create(instance.ClassName);

   if (instance.Instance) {
      instance.Instance->OnCreate(cubeEntity);
      data->Scripts.push_back(instance);
      }
   }

 void UILayer::CreateDefaultLight() {
    auto lightEntity = m_Scene.CreateEntity("Default Light");
    EntityData* data = m_Scene.GetEntityData(lightEntity.GetID());
    data->Light = new LightComponent(LightType::Directional, glm::vec3(1.0f, 1.0f, 1.0f), 1.0f);
    }


 void UILayer::TogglePlayMode() {
    m_PlayMode = !m_PlayMode;

    Scene* activeScene = nullptr;

    if (m_PlayMode) {
       activeScene = m_Scene.m_RuntimeScene.get();
       }
    else {
       activeScene = &m_Scene;
       }

    // Update all panels to use the current active context
    m_SceneHierarchyPanel.SetContext(activeScene);
    m_InspectorPanel.SetContext(activeScene);
    m_ViewportPanel.SetContext(activeScene);
    }

void UILayer::DeferSceneLoad(const std::string& filepath) {
    m_DeferredScenePath = filepath;
    m_HasDeferredSceneLoad = true;
}

void UILayer::ProcessDeferredSceneLoad() {
    if (m_HasDeferredSceneLoad) {
        std::cout << "[UILayer] Processing deferred scene load: " << m_DeferredScenePath << std::endl;
        
        if (Serializer::LoadSceneFromFile(m_DeferredScenePath, m_Scene)) {
            std::cout << "[UILayer] Successfully loaded scene: " << m_DeferredScenePath << std::endl;
            m_SelectedEntity = -1; // Clear selection
        } else {
            std::cerr << "[UILayer] Failed to load scene: " << m_DeferredScenePath << std::endl;
        }
        
        m_HasDeferredSceneLoad = false;
        m_DeferredScenePath.clear();
    }
}


