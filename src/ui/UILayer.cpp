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
#include "editor/panels/AnimationInspector.h"
#include "editor/animation/AnimationTimelinePanel.h"
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
#include "imnodes.h"
#include <editor/Input.h>
#include "serialization/Serializer.h"
#include <ImGuizmo.h>
#include <navigation/NavDebugDraw.h>
#include "panels/PrefabEditorPanel.h"
#include "panels/CodeEditorPanel.h"

// Forward-declare file dialog helper from MenuBarPanel.cpp
extern std::string ShowSaveFileDialog(const std::string& defaultName);
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
      m_MenuBarPanel(&m_Scene, &m_SelectedEntity, &m_ProjectPanel, this),
      m_AvatarBuilderPanel(&m_Scene)
{
    m_ToolbarPanel = ToolbarPanel(this);
    // Initialize global ImNodes context once
    ImNodes::CreateContext();

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

    m_AnimationInspector = std::make_unique<AnimationInspectorPanel>(this);
}

UILayer::~UILayer() {
    // Destroy global ImNodes context
    ImNodes::DestroyContext();
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
    // Wire node editor to inspector for selection details
    m_AnimCtrlPanel.SetInspectorPanel(&m_InspectorPanel);
    // New timeline panel owns its own inspector; legacy inspector-timeline wiring removed
    m_InspectorPanel.SetAvatarBuilderPanel(&m_AvatarBuilderPanel);
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
void UILayer::ExpandHierarchyTo(EntityID id) {
    m_SceneHierarchyPanel.ExpandTo(id);
}

void UILayer::OnUIRender() {
    BeginDockspace();

    // Determine which scene should be considered "active" for editor panels
    Scene* activeScene = m_PlayMode && m_Scene.m_RuntimeScene ? m_Scene.m_RuntimeScene.get() : &m_Scene;

    // Sticky routing: prefer last chosen editor source (prefab or main) until another editor window becomes active
    if (!m_ActiveEditorScene) {
        m_ActiveEditorScene = activeScene;
        m_ActiveSelectedEntityPtr = &m_SelectedEntity;
    }
    // Always keep main viewport bound to main scene
    m_ViewportPanel.SetContext(activeScene);

    // Prepare default routing to sticky source
    m_SceneHierarchyPanel.SetContext(m_ActiveEditorScene);
    m_SceneHierarchyPanel.SetSelectedEntityPtr(m_ActiveSelectedEntityPtr ? m_ActiveSelectedEntityPtr : &m_SelectedEntity);
    m_InspectorPanel.SetContext(m_ActiveEditorScene);
    m_InspectorPanel.SetSelectedEntityPtr(m_ActiveSelectedEntityPtr ? m_ActiveSelectedEntityPtr : &m_SelectedEntity);

    // Render other panels first
    m_ProjectPanel.OnImGuiRender();
    m_ConsolePanel.OnImGuiRender();
    m_ProfilerPanel.OnImGuiRender();
    if (m_FocusConsoleNextFrame) {
        ImGui::SetWindowFocus("Console");
        m_FocusConsoleNextFrame = false;
    }

    m_ScriptPanel.OnImGuiRender();
    m_AnimCtrlPanel.OnImGuiRender();
    m_AnimTimelinePanel.SetContext(activeScene, &m_SelectedEntity);
    m_AnimTimelinePanel.OnImGuiRender();
    // Avatar Builder (opens as a standalone window when requested)
    m_AvatarBuilderPanel.OnImGuiRender();

    // Main viewport
    {
        // Build display title: scene name or Untitled + '*' if dirty
        std::string sceneName = "Untitled";
        if (!m_CurrentScenePath.empty()) sceneName = fs::path(m_CurrentScenePath).stem().string();
        if (m_Scene.IsDirty()) sceneName += "*";
        m_ViewportPanel.SetDisplaySceneTitle(sceneName);
        m_ViewportPanel.OnImGuiRender(Renderer::Get().GetSceneTexture());
    }

    // If a blocking overlay is active (loading scene/play mode), render it now
    RenderBlockingOverlay();

    // Service async begin-play request after UI has a chance to paint the overlay
    ProcessBeginPlayAsync();

    // Global shortcuts scoped to active 3D editing surface: main viewport or any prefab viewport
    {
        // Drive shortcuts from the viewport's own focus/hover tracker
        bool viewportActive = m_ViewportPanel.IsWindowFocusedOrHovered();
        // If sticky source is prefab editor, keep shortcuts active for it only when that editor window is focused
        if (viewportActive) {
            bool ctrl = ImGui::GetIO().KeyCtrl;
            if (ctrl && Input::WasKeyPressedThisFrame(GLFW_KEY_S)) {
                // Route to active editor in this dockspace; default to scene
                bool handled = false;
                // Try focused prefab editor first
                for (auto& pe : m_PrefabEditors) {
                    if (pe && pe->IsWindowFocusedOrHovered()) {
                        // Save prefab subtree if possible
                        if (auto* ed = pe->GetScene()) {
                            EntityID* sel = pe->GetSelectedEntityPtr();
                            if (sel && *sel != -1) {
                                if (Serializer::SavePrefabSubtreeToFile(*ed, *sel, pe->GetPrefabPath())) {
                                    pe->ClearDirty();
                                }
                            }
                        }
                        handled = true; break;
                    }
                }
                // Try focused code editor
                if (!handled) {
                    for (auto& ce : m_CodeEditors) {
                        if (ce && ce->IsWindowFocusedOrHovered()) {
                            // Save through panel API
                            // We can't call private SaveFile; rely on menu exposure or re-open to force save; instead, emulate Ctrl+S via menu: handled here by menu or Render
                            // As a fallback, do nothing special; the code editor menu also listens to Ctrl+S
                            handled = true; break;
                        }
                    }
                }
                if (!handled) {
                    if (!m_CurrentScenePath.empty()) {
                        if (Serializer::SaveSceneToFile(*activeScene, m_CurrentScenePath)) {
                            m_Scene.ClearDirty();
                        }
                    } else {
                        // Prompt for save location
                        std::string chosen = ShowSaveFileDialog("NewScene.scene");
                        if (!chosen.empty() && Serializer::SaveSceneToFile(*activeScene, chosen)) {
                            m_CurrentScenePath = chosen;
                            m_Scene.ClearDirty();
                        }
                    }
                }
            }
        }
    }

    // Global Delete: remove selected entity when not editing text
    if (!ImGui::IsAnyItemActive() && m_SelectedEntity != -1 && Input::WasKeyPressedThisFrame(GLFW_KEY_DELETE)) {
        activeScene->QueueRemoveEntity(m_SelectedEntity);
        m_SelectedEntity = -1;
    }

    // Any open prefab editors. If one is focused/hovered, make it the sticky source for hierarchy/inspector.
    bool madeStickyThisFrame = false;
    for (auto it = m_PrefabEditors.begin(); it != m_PrefabEditors.end(); ) {
        PrefabEditorPanel* panel = it->get();
        panel->OnImGuiRender();
        bool wantsFocus = panel->IsWindowFocusedOrHovered();
        if (wantsFocus && !madeStickyThisFrame) {
            m_ActiveEditorScene = panel->GetScene();
            m_ActiveSelectedEntityPtr = panel->GetSelectedEntityPtr();
            m_SceneHierarchyPanel.SetContext(m_ActiveEditorScene);
            m_SceneHierarchyPanel.SetSelectedEntityPtr(m_ActiveSelectedEntityPtr);
            m_InspectorPanel.SetContext(m_ActiveEditorScene);
            m_InspectorPanel.SetSelectedEntityPtr(m_ActiveSelectedEntityPtr);
            madeStickyThisFrame = true;
        }
        if (!panel->IsOpen()) {
            if (m_ActiveEditorScene == panel->GetScene() || m_ActiveSelectedEntityPtr == panel->GetSelectedEntityPtr()) {
                m_ActiveEditorScene = activeScene;
                m_ActiveSelectedEntityPtr = &m_SelectedEntity;
                m_SceneHierarchyPanel.SetContext(m_ActiveEditorScene);
                m_SceneHierarchyPanel.SetSelectedEntityPtr(m_ActiveSelectedEntityPtr);
                m_InspectorPanel.SetContext(m_ActiveEditorScene);
                m_InspectorPanel.SetSelectedEntityPtr(m_ActiveSelectedEntityPtr);
            }
            it = m_PrefabEditors.erase(it);
        } else {
            ++it;
        }
    }

    // Render open code editors
    for (auto it = m_CodeEditors.begin(); it != m_CodeEditors.end(); ) {
        auto* panel = it->get();
        panel->OnImGuiRender();
        if (!panel->IsWindowFocusedOrHovered()) {
            // keep open; no explicit close tracking impl yet
        }
        ++it;
    }
    // If the main viewport window is focused, switch sticky source back to main scene
    if (m_ViewportPanel.IsWindowFocusedOrHovered()) {
        m_ActiveEditorScene = activeScene;
        m_ActiveSelectedEntityPtr = &m_SelectedEntity;
        m_SceneHierarchyPanel.SetContext(m_ActiveEditorScene);
        m_SceneHierarchyPanel.SetSelectedEntityPtr(m_ActiveSelectedEntityPtr);
        m_InspectorPanel.SetContext(m_ActiveEditorScene);
        m_InspectorPanel.SetSelectedEntityPtr(m_ActiveSelectedEntityPtr);
    }

    // If no prefab editor claimed focus this frame, keep whatever sticky source we had
    // Now render the shared Scene Hierarchy and Inspector with the chosen context
    // Route Inspector to Animation inspector when a .anim is selected in Project panel
    m_SceneHierarchyPanel.OnImGuiRender();
    {
        // Forward currently selected asset path to Inspector so it can show previews
        m_InspectorPanel.SetSelectedAssetPath(m_ProjectPanel.GetSelectedItemPath());
        std::string selExt = m_ProjectPanel.GetSelectedItemExtension();
        if (selExt == ".anim") {
            ImGui::Begin("Inspector");
            if (m_AnimationInspector) m_AnimationInspector->OnImGuiRender();
            ImGui::End();
        } else {
            m_InspectorPanel.OnImGuiRender();
        }
        // Render Asset Registry panel docked in the same right column
        m_AssetRegistryPanel.OnImGuiRender();
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
        ImGui::DockBuilderDockWindow("Asset Registry", dock_right);
        // Dock the main viewport using its dynamic name with a stable ID so it becomes the leading tab
        {
            std::string sceneName = m_CurrentScenePath.empty() ? std::string("Untitled") : fs::path(m_CurrentScenePath).stem().string();
            if (m_Scene.IsDirty()) sceneName += "*";
            std::string uniqueViewportName = sceneName + " - Viewport###Viewport";
            ImGui::DockBuilderDockWindow(uniqueViewportName.c_str(), dockspace_id);
        }
        ImGui::DockBuilderDockWindow("Animation Controller", dockspace_id);
        ImGui::DockBuilderDockWindow("Animation Timeline", dockspace_id);
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

    // Load toolbar icons once
    static bool sToolbarIconsLoaded = false;
    static ImTextureID sPlayIcon;
    static ImTextureID sPauseIcon;
    static ImTextureID sStopIcon;
    if (!sToolbarIconsLoaded) {
        sPlayIcon  = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/play.svg"));
        sPauseIcon = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/pause.svg"));
        sStopIcon  = TextureLoader::ToImGuiTextureID(TextureLoader::LoadIconTexture("assets/icons/stop.svg"));
        sToolbarIconsLoaded = true;
    }

    // Button sizing and centering of the 3-icon group
    ImVec2 iconSize(18, 18);
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float groupWidth = iconSize.x * 3.0f + spacing * 2.0f;
    float avail = ImGui::GetContentRegionAvail().x;
    float offsetX = (avail - groupWidth) * 0.5f;
    if (offsetX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

    bool isPlaying = m_ToolbarPanel.IsPlayMode();
    bool isPaused = m_ToolbarPanel.IsPaused();

    // Helper lambdas for coloring states
    auto pushUnavailable = [](){ ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.13f, 0.90f)); ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.14f, 0.16f, 0.90f)); ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.16f, 0.18f, 0.90f)); };
    auto pushActiveLight = [](){ ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.32f, 0.36f, 0.95f)); ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.33f, 0.35f, 0.39f, 0.95f)); ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.36f, 0.38f, 0.42f, 0.95f)); };
    auto popColors = [](){ ImGui::PopStyleColor(3); };

    // Play button
    if (isPlaying) {
        // Unavailable while playing
        ImGui::BeginDisabled(true);
        pushUnavailable();
        ImGui::ImageButton("##play", sPlayIcon, iconSize);
        popColors();
        ImGui::EndDisabled();
    } else {
        // Available in edit mode
        if (ImGui::ImageButton("##play", sPlayIcon, iconSize)) {
            m_ToolbarPanel.TogglePlayMode();
        }
    }

    ImGui::SameLine();

    // Pause button (only available in play mode)
    if (!isPlaying) {
        ImGui::BeginDisabled(true);
        pushUnavailable();
        ImGui::ImageButton("##pause", sPauseIcon, iconSize);
        popColors();
        ImGui::EndDisabled();
    } else {
        if (isPaused) pushActiveLight();
        if (ImGui::ImageButton("##pause", sPauseIcon, iconSize)) {
            m_ToolbarPanel.TogglePause();
        }
        if (isPaused) popColors();
    }

    ImGui::SameLine();

    // Stop button (only available in play mode)
    if (!isPlaying) {
        ImGui::BeginDisabled(true);
        pushUnavailable();
        ImGui::ImageButton("##stop", sStopIcon, iconSize);
        popColors();
        ImGui::EndDisabled();
    } else {
        if (ImGui::ImageButton("##stop", sStopIcon, iconSize)) {
            m_ToolbarPanel.TogglePlayMode();
        }
    }

    // Right-aligned View dropdown (combo)
    {
        float rightRegion = ImGui::GetContentRegionAvail().x;
        if (rightRegion > 0) {
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + rightRegion - 140.0f);
        }
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::BeginCombo("##ViewOptions", "View")) {
            bool showGrid = Renderer::Get().GetShowGrid();
            bool showAABBs = Renderer::Get().GetShowAABBs();
            bool showColliders = Renderer::Get().GetShowColliders();
            if (ImGui::Checkbox("Debug Grid", &showGrid)) {
                Renderer::Get().SetShowGrid(showGrid);
            }
            if (ImGui::Checkbox("Picking AABBs", &showAABBs)) {
                Renderer::Get().SetShowAABBs(showAABBs);
            }
            if (ImGui::Checkbox("Colliders", &showColliders)) {
                Renderer::Get().SetShowColliders(showColliders);
            }
            // Navigation debug draw toggles
            {
                ImGui::Separator();
                ImGui::Text("Navigation Debug");
                uint32_t mask = (uint32_t)nav::debug::GetMask();
                auto toggle = [&](const char* label, nav::NavDrawMask bit){ bool v = (mask & (uint32_t)bit)!=0; if(ImGui::Checkbox(label, &v)){ if(v) mask |= (uint32_t)bit; else mask &= ~(uint32_t)bit; }};
                toggle("Triangles", nav::NavDrawMask::TriMesh);
                toggle("Polys",     nav::NavDrawMask::Polys);
                toggle("BVTree",    nav::NavDrawMask::BVTree);
                toggle("Path",      nav::NavDrawMask::Path);
                toggle("Links",     nav::NavDrawMask::Links);
                toggle("Agents",    nav::NavDrawMask::Agents);
                nav::Navigation::Get().SetDebugMask((nav::NavDrawMask)mask);
            }
            ImGui::EndCombo();
        }
    }

    // Right-aligned View/Gizmo options
    ImGui::SameLine();
    float rightAvail = ImGui::GetContentRegionAvail().x;
    if (rightAvail > 0) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + rightAvail - 220.0f);
    }
    bool showGizmos = m_ToolbarPanel.IsShowGizmosEnabled();
    if (ImGui::Checkbox("Gizmos", &showGizmos)) {
        // Apply to main viewport
        m_ViewportPanel.SetShowGizmos(showGizmos);
        // Apply to any prefab editor viewports
        for (auto& pe : m_PrefabEditors) { (void)pe; }
        // Persist in toolbar state
        m_ToolbarPanel.SetShowGizmosEnabled(showGizmos);
    }
    ImGui::SameLine();

    // Gizmo operation quick toggles (T/R/S)
    {
        constexpr float btnSize = 22.0f;
        auto drawOpBtn = [&](ImGuizmo::OPERATION op, const char* label) {
            bool active = (m_ViewportPanel.GetCurrentOperation() == op);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.55f, 0.92f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.58f, 0.96f, 1.0f));
            }
            if (ImGui::Button(label, ImVec2(btnSize, btnSize))) {
                m_ViewportPanel.SetOperation(op);
            }
            if (active) {
                ImGui::PopStyleColor(2);
            }
            ImGui::SameLine();
        };
        drawOpBtn(ImGuizmo::TRANSLATE, "T");
        drawOpBtn(ImGuizmo::ROTATE,    "R");
        drawOpBtn(ImGuizmo::SCALE,     "S");
    }

    if (ImGui::BeginCombo("##GizmoMode", "Options")) {
        // Additional toggles can be added here in the future
        bool grid = Renderer::Get().GetShowGrid();
        if (ImGui::Checkbox("Debug Grid", &grid)) Renderer::Get().SetShowGrid(grid);
        bool aabbs = Renderer::Get().GetShowAABBs();
        if (ImGui::Checkbox("Picking AABBs", &aabbs)) Renderer::Get().SetShowAABBs(aabbs);
        bool colliders = Renderer::Get().GetShowColliders();
        if (ImGui::Checkbox("Colliders", &colliders)) Renderer::Get().SetShowColliders(colliders);
        bool uirects = Renderer::Get().GetShowUIRects();
        if (ImGui::Checkbox("UI Rects", &uirects)) Renderer::Get().SetShowUIRects(uirects);
        // Nav toggles
        uint32_t mask = (uint32_t)nav::debug::GetMask();
        auto toggle = [&](const char* label, nav::NavDrawMask bit){ bool v = (mask & (uint32_t)bit)!=0; if(ImGui::Checkbox(label, &v)){ if(v) mask |= (uint32_t)bit; else mask &= ~(uint32_t)bit; }};
        toggle("Nav Triangles", nav::NavDrawMask::TriMesh);
        toggle("Nav Polys",     nav::NavDrawMask::Polys);
        toggle("Nav Agents",    nav::NavDrawMask::Agents);
        nav::Navigation::Get().SetDebugMask((nav::NavDrawMask)mask);
        ImGui::EndCombo();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Secondary Gizmo bar directly below the main toolbar
    ImGui::Separator();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
    ImGui::BeginChild("GizmoBar", ImVec2(0, 32), false, ImGuiWindowFlags_NoScrollbar);

    bool gizmos = m_ViewportPanel.GetShowGizmos();
    if (ImGui::Checkbox("Gizmos", &gizmos)) {
        m_ViewportPanel.SetShowGizmos(gizmos);
        m_ToolbarPanel.SetShowGizmosEnabled(gizmos);
        for (auto& pe : m_PrefabEditors) { (void)pe; }
    }
    ImGui::SameLine();

    // Operation buttons (T/R/S)
    constexpr float kGizmoBtn = 22.0f;
    auto drawOpBtn2 = [&](ImGuizmo::OPERATION op, const char* label) {
        bool active = (m_ViewportPanel.GetCurrentOperation() == op);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.55f, 0.92f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.58f, 0.96f, 1.0f));
        }
        if (ImGui::Button(label, ImVec2(kGizmoBtn, kGizmoBtn))) {
            m_ViewportPanel.SetOperation(op);
        }
        if (active) ImGui::PopStyleColor(2);
        ImGui::SameLine();
    };
    drawOpBtn2(ImGuizmo::TRANSLATE, "T");
    drawOpBtn2(ImGuizmo::ROTATE,    "R");
    drawOpBtn2(ImGuizmo::SCALE,     "S");

    // Optional: quick view options combo
    if (ImGui::BeginCombo("##ViewOpts2", "Options")) {
        bool grid2 = Renderer::Get().GetShowGrid();
        if (ImGui::Checkbox("Debug Grid", &grid2)) Renderer::Get().SetShowGrid(grid2);
        bool aabbs2 = Renderer::Get().GetShowAABBs();
        if (ImGui::Checkbox("Picking AABBs", &aabbs2)) Renderer::Get().SetShowAABBs(aabbs2);
        bool colliders2 = Renderer::Get().GetShowColliders();
        if (ImGui::Checkbox("Colliders", &colliders2)) Renderer::Get().SetShowColliders(colliders2);
        bool uirects2 = Renderer::Get().GetShowUIRects();
        if (ImGui::Checkbox("UI Rects", &uirects2)) Renderer::Get().SetShowUIRects(uirects2);
        uint32_t mask2 = (uint32_t)nav::debug::GetMask();
        auto toggle2 = [&](const char* label, nav::NavDrawMask bit){ bool v = (mask2 & (uint32_t)bit)!=0; if(ImGui::Checkbox(label, &v)){ if(v) mask2 |= (uint32_t)bit; else mask2 &= ~(uint32_t)bit; }};
        toggle2("Nav Triangles", nav::NavDrawMask::TriMesh);
        toggle2("Nav Polys",     nav::NavDrawMask::Polys);
        toggle2("Nav Agents",    nav::NavDrawMask::Agents);
        nav::Navigation::Get().SetDebugMask((nav::NavDrawMask)mask2);
        ImGui::EndCombo();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Render any modals requested by menu items after the menu bar has closed
    m_MenuBarPanel.RenderExportPopup();

    // DockSpace (below toolbar), reserve space for status bar using negative height
    ImGui::Separator();
    const float statusBarHeight = ImGui::GetFrameHeight();
    ImGuiID rootDockspaceId = m_MainDockspaceID;
    if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(m_MainDockspaceID)) {
        while (node->ParentNode != nullptr) node = node->ParentNode;
        rootDockspaceId = node->ID;
    }
    ImGui::DockSpace(rootDockspaceId, ImVec2(0.0f, -statusBarHeight), ImGuiDockNodeFlags_PassthruCentralNode);

    // Ensure the viewport tab is focused initially
    if (!m_PlayMode && !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
        std::string sceneName = m_CurrentScenePath.empty() ? std::string("Untitled") : fs::path(m_CurrentScenePath).stem().string();
        if (m_Scene.IsDirty()) sceneName += "*";
        std::string unique = sceneName + " - Viewport###Viewport";
        ImGui::SetWindowFocus(unique.c_str());
    }

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
    data->Mesh = std::make_unique<MeshComponent>(
        StandardMeshManager::Instance().GetCubeMesh(),
        std::string("DebugCube"),
        nullptr);
    data->Mesh->material = MaterialManager::Instance().CreateDefaultPBRMaterial();

}

void UILayer::CreateDefaultLight() {
    auto lightEntity = m_Scene.CreateEntity("Default Light");
    EntityData* data = m_Scene.GetEntityData(lightEntity.GetID());
    data->Light = std::make_unique<LightComponent>(LightType::Directional, glm::vec3(1.0f), 1.0f);
}

// =============================
// Play Mode Toggle
// =============================
void UILayer::TogglePlayMode() {
    m_PlayMode = !m_PlayMode;
    Scene* activeScene = m_PlayMode ? m_Scene.m_RuntimeScene.get() : &m_Scene;

    // Update panel contexts
    // Also reset sticky routing to avoid dangling pointers to the destroyed runtime scene
    m_ActiveEditorScene = activeScene;
    m_ActiveSelectedEntityPtr = &m_SelectedEntity;
    m_SceneHierarchyPanel.SetContext(activeScene);
    m_SceneHierarchyPanel.SetSelectedEntityPtr(m_ActiveSelectedEntityPtr);
    m_InspectorPanel.SetContext(activeScene);
    m_ViewportPanel.SetContext(activeScene);
}

// =============================
// Prefab Editor Management
// =============================
void UILayer::OpenPrefabEditor(const std::string& prefabPath) {
    // If an editor for this prefab already exists, focus it instead of opening another
    for (auto& ed : m_PrefabEditors) {
        if (ed && ed->GetPrefabPath() == prefabPath) {
            ed->RequestFocus();
            return;
        }
    }
    m_PrefabEditors.emplace_back(std::make_unique<PrefabEditorPanel>(prefabPath, this));
}

void UILayer::OpenCodeEditor(const std::string& filePath) {
    // If already open, focus
    for (auto& ed : m_CodeEditors) {
        if (ed && ed->GetFilePath() == filePath) { ed->RequestFocus(); return; }
    }
    m_CodeEditors.emplace_back(std::make_unique<CodeEditorPanel>(filePath, this));
}

// =============================
// Deferred Scene Loading
// =============================
void UILayer::DeferSceneLoad(const std::string& filepath) {
    m_DeferredScenePath = filepath;
    m_HasDeferredSceneLoad = true;
    BeginBlockingOverlay("Loading Scene...");
}

void UILayer::ProcessDeferredSceneLoad() {
    if (!m_HasDeferredSceneLoad) return;

    std::cout << "[UILayer] Processing deferred scene load: " << m_DeferredScenePath << std::endl;
    if (Serializer::LoadSceneFromFile(m_DeferredScenePath, m_Scene)) {
        std::cout << "[UILayer] Successfully loaded scene: " << m_DeferredScenePath << std::endl;
        m_SelectedEntity = -1;
        m_CurrentScenePath = m_DeferredScenePath;
        // Reset viewport camera and interaction state after reload to ensure gizmo can capture input
        m_ViewportPanel.ClearPickRequest();
    } else {
        std::cerr << "[UILayer] Failed to load scene: " << m_DeferredScenePath << std::endl;
    }
    m_HasDeferredSceneLoad = false;
    m_DeferredScenePath.clear();
    EndBlockingOverlay();
}

void UILayer::BeginBlockingOverlay(const std::string& label) {
    m_BlockingOverlayActive = true;
    m_BlockingOverlayLabel = label;
}

void UILayer::EndBlockingOverlay() {
    m_BlockingOverlayActive = false;
    m_BlockingOverlayLabel.clear();
}

void UILayer::RenderBlockingOverlay() {
    if (!m_BlockingOverlayActive) return;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0.35f));
    ImGui::Begin("##BlockingOverlay", nullptr, flags);
    // Center box
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 box(360, 120);
    ImVec2 cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cursor.x + (avail.x - box.x)*0.5f, cursor.y + (avail.y - box.y)*0.5f));
    ImGui::BeginChild("##LoadingBox", box, true, ImGuiWindowFlags_NoScrollbar);
    ImGui::Text("%s", m_BlockingOverlayLabel.empty()? "Loading..." : m_BlockingOverlayLabel.c_str());
    ImGui::Separator();
    // Indeterminate bar alternative
    static float t = 0.0f; t += 0.02f; if (t > 1.0f) t = 0.0f;
    ImGui::ProgressBar(t, ImVec2(-1, 0));
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// Queue async play begin so overlay can draw before heavy work
void UILayer::RequestBeginPlayAsync() {
    BeginBlockingOverlay("Starting Play Mode...");
    m_BeginPlayRequested = true;
}

void UILayer::ProcessBeginPlayAsync() {
    if (!m_BeginPlayRequested) return;
    // Perform the heavy work now that at least one UI frame has rendered the overlay
    auto& scene = m_Scene;
    scene.m_RuntimeScene = scene.RuntimeClone();
    if (scene.m_RuntimeScene) {
        scene.m_RuntimeScene->m_IsPlaying = true;
        TogglePlayMode();
    }
    EndBlockingOverlay();
    m_BeginPlayRequested = false;
}
