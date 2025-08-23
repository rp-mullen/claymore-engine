#pragma once
#include <bgfx/bgfx.h>
#include <imgui.h>
#include <functional>

// Function type for converting ImGuiViewport* to a native window handle for bgfx
typedef std::function<void*(ImGuiViewport*)> ImGuiBgfx_ViewportHandleConverter;

/**
 * @brief Initializes the ImGui BGFX renderer backend with multi-viewport support.
 *
 * @param mainViewId The main BGFX view ID used for ImGui rendering.
 * @param converter Lambda or function that converts ImGuiViewport* to native OS handle.
 *
 * Example (GLFW + Win32):
 * ImGui_ImplBgfx_Init(255, [](ImGuiViewport* vp) {
 *     return glfwGetWin32Window((GLFWwindow*)vp->PlatformHandle);
 * });
 */
void ImGui_ImplBgfx_Init(int mainViewId);

/**
 * @brief Shutdown and release BGFX resources allocated by the ImGui renderer backend.
 */
void ImGui_ImplBgfx_Shutdown();

/**
 * @brief Prepare ImGui BGFX renderer for a new frame.
 *
 * Must be called after ImGui_ImplGlfw_NewFrame() and before ImGui::NewFrame().
 */
void ImGui_ImplBgfx_NewFrame();

/**
 * @brief Render ImGui draw data into the specified BGFX view.
 *
 * @param viewId The BGFX view ID.
 * @param draw_data The ImGui draw data (usually ImGui::GetDrawData()).
 * @param clearColor Optional clear color in BGRA8 format (e.g., 0x303030ff).
 */
void ImGui_ImplBgfx_Render(bgfx::ViewId viewId, ImDrawData* draw_data, uint32_t clearColor = 0);

/**
 * @brief Recreate device objects (shaders, font textures).
 * Called automatically by ImGui_ImplBgfx_NewFrame() if necessary.
 */
void ImGui_ImplBgfx_CreateDeviceObjects();

/**
 * @brief Invalidate device objects (free GPU resources).
 */
void ImGui_ImplBgfx_InvalidateDeviceObjects();
