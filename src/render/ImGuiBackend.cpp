#include "render/ImGuiBackend.hpp"
#include "core/Input.hpp"
#include "core/Log.hpp"
#include "core/Config.hpp"
#include "ui/Theme.hpp"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace sfc {
namespace {
bool g_ready = false;
HWND g_hwnd = nullptr;
WNDPROC g_oldWnd = nullptr;
IDirect3DDevice9* g_device = nullptr;
bool g_wasWantCapture = false;

void ClipToGameWindow()
{
	if (!g_hwnd) return;
	RECT rc{};
	if (!GetClientRect(g_hwnd, &rc)) return;
	POINT tl{ rc.left, rc.top };
	POINT br{ rc.right, rc.bottom };
	ClientToScreen(g_hwnd, &tl);
	ClientToScreen(g_hwnd, &br);
	RECT clip{ tl.x, tl.y, br.x, br.y };
	ClipCursor(&clip);
}

void HideOsCursor()
{
	int guard = 0;
	int counter = ShowCursor(FALSE);
	while (counter >= 0 && guard++ < 16)
		counter = ShowCursor(FALSE);
	SetCursor(nullptr);
}

void ReleaseMenuMouse()
{
	ClipCursor(nullptr);
	ReleaseCapture();
	HideOsCursor();
}

void FeedImGuiMouseFromOs()
{
	// FNV uses DirectInput — WndProc mouse often never reaches ImGui. Drive it ourselves.
	ImGuiIO& io = ImGui::GetIO();
	POINT pt{};
	if (GetCursorPos(&pt) && g_hwnd) {
		POINT client = pt;
		if (ScreenToClient(g_hwnd, &client)) {
			io.AddMousePosEvent(static_cast<float>(client.x), static_cast<float>(client.y));
		}
	}
	io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
	io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
	io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);
}

void UpdateMenuCursor()
{
	const bool want = Input::Get().WantCapture();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

	if (want) {
		if (!g_wasWantCapture) {
			ReleaseCapture();
			SFC_LOG("Menu cursor armed (locked to game window)");
		}
		// ONE cursor only: ImGui software cursor. Hide OS arrow (stops dual-cursor / 2nd-monitor mess).
		HideOsCursor();
		ClipToGameWindow();
		io.MouseDrawCursor = true;
	} else if (g_wasWantCapture) {
		io.MouseDrawCursor = false;
		ReleaseMenuMouse();
		SFC_LOG("Menu cursor released");
	} else {
		io.MouseDrawCursor = false;
	}

	g_wasWantCapture = want;
}

LRESULT CALLBACK SfcWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

	// Never leave the cursor clipped if the game loses focus (Alt-Tab / 2nd monitor).
	if (msg == WM_ACTIVATEAPP && wParam == FALSE) {
		ClipCursor(nullptr);
	}
	if (msg == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE) {
		ClipCursor(nullptr);
	}
	if (msg == WM_KILLFOCUS) {
		ClipCursor(nullptr);
	}

	if (Input::Get().WantCapture() && Config::Get().Data().controls.blockGameInputWhenMenuOpen) {
		switch (msg) {
		case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
		case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: case WM_MOUSEMOVE:
		case WM_INPUT:
		case WM_CAPTURECHANGED:
			return true;
		case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR:
		case WM_SYSKEYDOWN: case WM_SYSKEYUP:
			if (wParam == VK_INSERT || wParam == VK_ESCAPE || wParam == VK_F1)
				break;
			return true;
		case WM_SETCURSOR:
			// Keep OS cursor hidden while menu uses ImGui cursor only.
			SetCursor(nullptr);
			return true;
		default: break;
		}
	}
	return CallWindowProcA(g_oldWnd, hWnd, msg, wParam, lParam);
}
}

bool ImGuiBackend::Ready() { return g_ready; }

bool ImGuiBackend::Init(IDirect3DDevice9* device)
{
	if (g_ready || !device) return g_ready;
	g_device = device;

	D3DDEVICE_CREATION_PARAMETERS cp{};
	if (FAILED(device->GetCreationParameters(&cp))) return false;
	g_hwnd = cp.hFocusWindow;
	if (!g_hwnd) g_hwnd = GetForegroundWindow();
	if (!g_hwnd) return false;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = nullptr;
	io.MouseDrawCursor = false;

	ImGui_ImplWin32_Init(g_hwnd);
	ImGui_ImplDX9_Init(device);

	g_oldWnd = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SfcWndProc)));
	g_ready = true;
	Theme::Apply(Config::Get().Data().theme);
	SFC_LOG("[IMGUI] backend initialized (hwnd=%p)", g_hwnd);
	return true;
}

void ImGuiBackend::Shutdown()
{
	if (!g_ready) return;
	ClipCursor(nullptr);
	if (g_hwnd && g_oldWnd) {
		SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_oldWnd));
		g_oldWnd = nullptr;
	}
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	g_ready = false;
}

void ImGuiBackend::OnEndScene(IDirect3DDevice9* device)
{
	if (!g_ready) {
		Init(device);
		if (!g_ready) return;
	}

	IDirect3DSurface9* bb = nullptr;
	if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) && bb) {
		D3DSURFACE_DESC d{};
		bb->GetDesc(&d);
		bb->Release();
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(static_cast<float>(d.Width), static_cast<float>(d.Height));
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	}

	UpdateMenuCursor();
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	// After Win32 NewFrame so our DInput-friendly mouse wins for clicking.
	if (Input::Get().WantCapture())
		FeedImGuiMouseFromOs();
	ImGui::NewFrame();
}

void ImGuiBackend::Render()
{
	if (!g_ready) return;
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiBackend::OnLostDevice()
{
	if (!g_ready) return;
	ImGui_ImplDX9_InvalidateDeviceObjects();
}

void ImGuiBackend::OnResetDevice()
{
	if (!g_ready) return;
	ImGui_ImplDX9_CreateDeviceObjects();
}

} // namespace sfc
