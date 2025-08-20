#pragma once
#define NOMINMAX
#include <windows.h>
#include <functional>

// Forward declare ImGui Win32 handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Win32Window {
public:
	Win32Window() = default;
	~Win32Window() { Destroy(); }

	bool Create(const wchar_t* title, int width, int height, bool resizable = true, bool highDPI = true);
	void Destroy();
	void PumpEvents();

	// Callbacks
	void SetResizeCallback(const std::function<void(int,int,bool)>& cb) { m_OnResize = cb; }

	// Properties
	HWND GetHWND() const { return m_hWnd; }
	void* GetOSHandle() const { return (void*)m_hWnd; }
	int GetWidth() const { return m_Width; }
	int GetHeight() const { return m_Height; }
	bool IsMinimized() const { return m_Minimized; }
	bool ShouldClose() const { return m_ShouldClose; }
	float GetDPIScale() const;

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static int MapVKToGLFW(int vk);

private:
	HWND m_hWnd = nullptr;
	HINSTANCE m_hInstance = nullptr;
	int m_Width = 0;
   int m_Height = 0;
	bool m_Minimized = false;
	bool m_ShouldClose = false;
	bool m_HighDPI = true;
	std::function<void(int,int,bool)> m_OnResize;
};


