#include "Win32Window.h"
#include <windowsx.h>
#include <cstdio>
#include <algorithm>
#include <imgui.h>
#include "backends/imgui_impl_win32.h"
#include "editor/Input.h"

static Win32Window* g_WindowInstance = nullptr;

float Win32Window::GetDPIScale() const {
	if (!m_HighDPI || m_hWnd == nullptr) return 1.0f;
	UINT dpi = 96;
	// GetDpiForWindow is Win10+
	HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
	if (hUser32) {
		auto pGetDpiForWindow = reinterpret_cast<UINT(WINAPI*)(HWND)>(GetProcAddress(hUser32, "GetDpiForWindow"));
		if (pGetDpiForWindow) dpi = pGetDpiForWindow(m_hWnd);
	}
	return std::max(1.0f, static_cast<float>(dpi) / 96.0f);
}

bool Win32Window::Create(const wchar_t* title, int width, int height, bool resizable, bool highDPI) {
	m_HighDPI = highDPI;
	m_hInstance = GetModuleHandleW(nullptr);
	WNDCLASSEXW wc{ sizeof(wc) };
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = &Win32Window::WndProc;
	wc.hInstance = m_hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = L"ClaymoreWin32Window";
	if (!RegisterClassExW(&wc)) return false;

	DWORD style = WS_OVERLAPPEDWINDOW;
	if (!resizable) style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
	RECT r{ 0, 0, width, height };
	AdjustWindowRect(&r, style, FALSE);

	m_hWnd = CreateWindowExW(0, wc.lpszClassName, title, style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		r.right - r.left, r.bottom - r.top,
		nullptr, nullptr, m_hInstance, nullptr);
	if (!m_hWnd) return false;

	ShowWindow(m_hWnd, SW_SHOW);
	UpdateWindow(m_hWnd);
	SetForegroundWindow(m_hWnd);

	m_Width = width; m_Height = height; m_Minimized = false; m_ShouldClose = false;
	g_WindowInstance = this;
	return true;
}

void Win32Window::Destroy() {
	if (m_hWnd) {
		DestroyWindow(m_hWnd);
		m_hWnd = nullptr;
	}
}

void Win32Window::PumpEvents() {
	MSG msg;
	while (PeekMessageW(&msg, nullptr, 0u, 0u, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

int Win32Window::MapVKToGLFW(int vk) {
	// Minimal subset needed by current codebase
	// Letters map to ASCII capital letters in GLFW
	if (vk >= 'A' && vk <= 'Z') return vk;
	if (vk >= '0' && vk <= '9') return vk;
	// Delete key: GLFW_KEY_DELETE = 261
	if (vk == VK_DELETE) return 261;
	return vk; // Fallback: pass-through
}

LRESULT CALLBACK Win32Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return 1;

	switch (msg) {
		case WM_SIZE: {
			if (g_WindowInstance) {
				g_WindowInstance->m_Minimized = (wParam == SIZE_MINIMIZED);
				g_WindowInstance->m_Width = LOWORD(lParam);
				g_WindowInstance->m_Height = HIWORD(lParam);
				if (g_WindowInstance->m_OnResize) {
					g_WindowInstance->m_OnResize(g_WindowInstance->m_Width, g_WindowInstance->m_Height, g_WindowInstance->m_Minimized);
				}
			}
			return 0;
		}
		case WM_DPICHANGED: {
			RECT* const rc = reinterpret_cast<RECT*>(lParam);
			SetWindowPos(hWnd, nullptr, rc->left, rc->top,
					 rc->right - rc->left, rc->bottom - rc->top,
					 SWP_NOZORDER | SWP_NOACTIVATE);
			return 0;
		}
		case WM_MOUSEMOVE: {
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			Input::OnMouseMove((double)x, (double)y);
			return 0;
		}
		case WM_LBUTTONDOWN: case WM_LBUTTONUP:
		case WM_RBUTTONDOWN: case WM_RBUTTONUP:
		case WM_MBUTTONDOWN: case WM_MBUTTONUP: {
			int button = (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) ? 0 :
				(msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) ? 1 : 2;
			int action = (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN) ? 1 : 0; // GLFW_PRESS=1, RELEASE=0
			Input::OnMouseButton(button, action);
			return 0;
		}
		case WM_MOUSEWHEEL: {
			float dy = (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
			Input::OnScroll(dy);
			return 0;
		}
		case WM_KEYDOWN: case WM_SYSKEYDOWN:
		case WM_KEYUP:   case WM_SYSKEYUP: {
			bool down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
			int vk = (int)wParam;
			int mapped = MapVKToGLFW(vk);
			Input::OnKey(mapped, down ? 1 : 0);
			return 0;
		}
		case WM_CHAR: {
			// If needed later, route text input to ImGui or custom text system
			return 0;
		}
		case WM_CLOSE: {
			if (g_WindowInstance) g_WindowInstance->m_ShouldClose = true;
			PostQuitMessage(0);
			return 0;
		}
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}


