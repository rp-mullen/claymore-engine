#include "Win32Window.h"
#include <windowsx.h>
#include <cstdio>
#include <algorithm>
#include <vector>
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

	ShowWindow(m_hWnd, SW_SHOWMAXIMIZED);
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

void Win32Window::SetCursorCaptured(bool captured) {
	if (m_Captured == captured) return;
	m_Captured = captured;
	if (captured) {
		// Hide cursor and confine it to the window; use relative mouse input
		ShowCursor(FALSE);
		RECT rect; GetClientRect(m_hWnd, &rect);
		POINT tl = { rect.left, rect.top };
		POINT br = { rect.right, rect.bottom };
		ClientToScreen(m_hWnd, &tl);
		ClientToScreen(m_hWnd, &br);
		RECT clip = { tl.x, tl.y, br.x, br.y };
		ClipCursor(&clip);
		// Center the cursor in window
		POINT center = { (tl.x + br.x) / 2, (tl.y + br.y) / 2 };
		SetCursorPos(center.x, center.y);
		// Remember logical center in client coords for Input::GetMousePosition
		float cx = (float)(rect.right - rect.left) * 0.5f;
		float cy = (float)(rect.bottom - rect.top) * 0.5f;
		Input::SetLockedCenter(cx, cy);
		// Enable raw input for high-precision relative motion
		RAWINPUTDEVICE rid{}; rid.usUsagePage = 0x01; rid.usUsage = 0x02; rid.dwFlags = RIDEV_INPUTSINK; rid.hwndTarget = m_hWnd;
		RegisterRawInputDevices(&rid, 1, sizeof(rid));
	} else {
		ClipCursor(nullptr);
		ShowCursor(TRUE);
		// Optional: unregister raw input
		RAWINPUTDEVICE rid{}; rid.usUsagePage = 0x01; rid.usUsage = 0x02; rid.dwFlags = RIDEV_REMOVE; rid.hwndTarget = nullptr;
		RegisterRawInputDevices(&rid, 1, sizeof(rid));
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
		case WM_INPUT: {
			if (g_WindowInstance && g_WindowInstance->m_Captured) {
				UINT size = 0;
				GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
				if (size > 0) {
					std::vector<BYTE> buffer(size);
					if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) == size) {
						RAWINPUT* ri = reinterpret_cast<RAWINPUT*>(buffer.data());
						if (ri->header.dwType == RIM_TYPEMOUSE) {
							LONG dx = ri->data.mouse.lLastX;
							LONG dy = ri->data.mouse.lLastY;
							if (dx != 0 || dy != 0) {
								Input::OnMouseMove((double)dx, (double)dy);
							}
						}
					}
				}
				return 0;
			}
			break;
		}
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
			if (g_WindowInstance && g_WindowInstance->m_Captured) {
				// Ignore absolute mouse moves in captured mode; WM_INPUT provides deltas
				return 0;
			}
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


void Win32Window::EnterFullscreen() {
	if (m_Fullscreen || m_hWnd == nullptr) return;

	// Save current window state
	m_SavedMaximized = !!IsZoomed(m_hWnd);
	if (m_SavedMaximized) {
		SendMessage(m_hWnd, WM_SYSCOMMAND, SC_RESTORE, 0);
	}
	m_SavedStyle = GetWindowLong(m_hWnd, GWL_STYLE);
	m_SavedExStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
	GetWindowRect(m_hWnd, &m_SavedWindowRect);

	// Remove window decorations
	DWORD newStyle = m_SavedStyle & ~(WS_CAPTION | WS_THICKFRAME);
	DWORD newExStyle = m_SavedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
	SetWindowLong(m_hWnd, GWL_STYLE, newStyle);
	SetWindowLong(m_hWnd, GWL_EXSTYLE, newExStyle);

	// Resize to monitor bounds
	HMONITOR hmon = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi{ sizeof(mi) };
	if (GetMonitorInfo(hmon, &mi)) {
		SetWindowPos(m_hWnd, nullptr,
			mi.rcMonitor.left, mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left,
			mi.rcMonitor.bottom - mi.rcMonitor.top,
			SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	}

	m_Fullscreen = true;
}

void Win32Window::ExitFullscreen() {
	if (!m_Fullscreen || m_hWnd == nullptr) return;

	// Restore styles
	SetWindowLong(m_hWnd, GWL_STYLE, m_SavedStyle);
	SetWindowLong(m_hWnd, GWL_EXSTYLE, m_SavedExStyle);

	// Restore window size/pos
	SetWindowPos(m_hWnd, nullptr,
		m_SavedWindowRect.left, m_SavedWindowRect.top,
		m_SavedWindowRect.right - m_SavedWindowRect.left,
		m_SavedWindowRect.bottom - m_SavedWindowRect.top,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

	if (m_SavedMaximized) {
		SendMessage(m_hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
	}

	m_Fullscreen = false;
}

void Win32Window::ToggleFullscreen() {
	if (m_Fullscreen) ExitFullscreen();
	else EnterFullscreen();
}


