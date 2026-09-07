#include "render/D3D9Hook.hpp"
#include "render/ImGuiBackend.hpp"
#include "render/WorldToScreen.hpp"
#include "core/Log.hpp"
#include "core/Boot.hpp"
#include "core/App.hpp"
#include "core/Config.hpp"
#include "core/Gameplay.hpp"
#include "core/Input.hpp"
#include "core/Hotkeys.hpp"
#include "core/Diag.hpp"
#include "core/GameWorkQueue.hpp"
#include "core/Isolation.hpp"
#include "MinHook.h"
#include <windows.h>
#include <d3d9.h>
#include <cstring>

namespace sfc {
namespace {

using EndScene_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using Reset_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using CreateDevice_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
using Direct3DCreate9_t = IDirect3D9*(WINAPI*)(UINT);

EndScene_t oEndScene = nullptr;
Reset_t oReset = nullptr;
CreateDevice_t oCreateDevice = nullptr;
Direct3DCreate9_t oDirect3DCreate9 = nullptr;

IDirect3DDevice9* g_device = nullptr;
bool g_deviceHooks = false;
bool g_createHooks = false;
volatile bool g_shutdown = false;
volatile bool g_hardDisableDraw = false;
volatile bool g_rendering = false;
volatile bool g_inReset = false;
IDirect3DStateBlock9* g_stateBlock = nullptr;

void ReleaseStateBlock()
{
	if (g_stateBlock) {
		g_stateBlock->Release();
		g_stateBlock = nullptr;
	}
}

bool EnsureStateBlock(IDirect3DDevice9* device)
{
	if (g_stateBlock) return true;
	if (!device) return false;
	if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &g_stateBlock)) || !g_stateBlock) {
		g_stateBlock = nullptr;
		return false;
	}
	return true;
}

bool IsPrimaryBackBuffer(IDirect3DDevice9* device)
{
	IDirect3DSurface9* rt = nullptr;
	IDirect3DSurface9* bb = nullptr;
	if (FAILED(device->GetRenderTarget(0, &rt)) || !rt) {
		if (rt) rt->Release();
		return false;
	}
	if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) {
		rt->Release();
		if (bb) bb->Release();
		return false;
	}
	const bool ok = (rt == bb);
	rt->Release();
	bb->Release();
	return ok;
}

void RestoreTransforms(IDirect3DDevice9* device, const D3DMATRIX& world, const D3DMATRIX& view, const D3DMATRIX& proj)
{
	device->SetTransform(D3DTS_WORLD, &world);
	device->SetTransform(D3DTS_VIEW, &view);
	device->SetTransform(D3DTS_PROJECTION, &proj);
}

void CloseAllUi()
{
	if (Input::Get().MenuOpen()) Input::Get().SetMenuOpen(false);
	if (Input::Get().SearchOpen()) Input::Get().SetSearchOpen(false);
}

void DrawOverlaySafe(IDirect3DDevice9* device)
{
	// Draw only — gate already decided in hkEndScene from cached MainGameLoop state.
	if (g_inReset || IsLoadingScreen() || IsGameMenuBlocking())
		return;

	if (!EnsureStateBlock(device))
		return;
	if (FAILED(g_stateBlock->Capture())) {
		ReleaseStateBlock();
		return;
	}

	D3DVIEWPORT9 oldVp{};
	device->GetViewport(&oldVp);
	D3DMATRIX oldWorld{}, oldView{}, oldProj{};
	device->GetTransform(D3DTS_WORLD, &oldWorld);
	device->GetTransform(D3DTS_VIEW, &oldView);
	device->GetTransform(D3DTS_PROJECTION, &oldProj);

	if (Config::Get().Data().performance.espEnabled)
		CaptureCameraForFrame(device);

	ImGuiBackend::OnEndScene(device);
	if (App::Get().Ready())
		App::Get().OnFrame();
	ImGuiBackend::Render();

	RestoreTransforms(device, oldWorld, oldView, oldProj);
	device->SetViewport(&oldVp);
	if (g_stateBlock)
		g_stateBlock->Apply();
	RestoreTransforms(device, oldWorld, oldView, oldProj);
	device->SetViewport(&oldVp);
}

HRESULT STDMETHODCALLTYPE hkEndScene(IDirect3DDevice9* device)
{
	if (device && !g_shutdown && !g_inReset && App::Get().Ready() && !g_rendering) {
		auto& work = GameWorkQueue::Get();
		work.EnterRender();

		DiagThrottle("d3d.endscene", 5000, "EndScene");

		const bool loading = IsLoadingScreen();
		const bool gameUi = IsGameMenuBlocking();

		if (loading || gameUi)
			CloseAllUi();

		Input::Get().Tick();

		if (IsolationStaticImGuiOnly())
			CloseAllUi();

		// Fallback only if NVSE MainGameLoop missing (keeps cheats alive).
		if (!work.GameLoopAvailable())
			App::Get().TickGameWorld();

		// Prefer cached gate from MainGameLoop — avoid player/cell walks every EndScene.
		bool canDraw = !g_hardDisableDraw && !loading && !gameUi && OverlayGateCached();
		if (IsolationStaticImGuiOnly())
			canDraw = !g_hardDisableDraw && !loading && !gameUi && ObserveWorldReady();

		const bool menuOpen = Input::Get().WantCapture();
		if (canDraw && IsolationAllowHotkeys())
			Hotkeys::Get().Tick(); // console cmds enqueue only

		const bool liveHud = Config::Get().Data().hud.liveHud && Config::Get().Data().hud.enabled;
		const bool wantUi = IsolationStaticImGuiOnly() ? liveHud : (menuOpen || liveHud);

		if (canDraw && wantUi && IsPrimaryBackBuffer(device)) {
			g_rendering = true;
			g_device = device;
			__try {
				DrawOverlaySafe(device);
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				SFC_ERR("[HUD] overlay draw fault — frame skipped");
				ReleaseStateBlock();
				CloseAllUi();
			}
			g_rendering = false;
		}

		work.LeaveRender();
	}
	return oEndScene ? oEndScene(device) : D3DERR_INVALIDCALL;
}

HRESULT STDMETHODCALLTYPE hkReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params)
{
	g_inReset = true;
	int spin = 0;
	while (g_rendering && spin++ < 1000)
		Sleep(0);

	DiagEvent("d3d.reset", "begin");
	BootMark("D3D9", "Reset begin");
	CloseAllUi();
	ReleaseStateBlock();
	ImGuiBackend::OnLostDevice();
	const HRESULT hr = oReset ? oReset(device, params) : D3DERR_INVALIDCALL;
	if (SUCCEEDED(hr))
		ImGuiBackend::OnResetDevice();
	else
		SFC_WARN("[D3D9] Reset failed hr=0x%08X", static_cast<unsigned>(hr));
	ResetWorldSettle("d3d_reset");
	BootMark("D3D9", "Reset end");
	DiagEvent("d3d.reset", SUCCEEDED(hr) ? "ok" : "fail");
	g_inReset = false;
	return hr;
}

bool HookDevice(IDirect3DDevice9* device)
{
	if (!device || g_deviceHooks) return g_deviceHooks;

	void** vtable = *reinterpret_cast<void***>(device);
	if (MH_CreateHook(vtable[42], &hkEndScene, reinterpret_cast<void**>(&oEndScene)) != MH_OK) {
		SFC_ERR("[D3D9] EndScene hook create failed");
		return false;
	}
	if (MH_CreateHook(vtable[16], &hkReset, reinterpret_cast<void**>(&oReset)) != MH_OK) {
		SFC_ERR("[D3D9] Reset hook create failed");
		return false;
	}
	if (MH_EnableHook(vtable[42]) != MH_OK || MH_EnableHook(vtable[16]) != MH_OK) {
		SFC_ERR("[D3D9] Device hook enable failed");
		return false;
	}

	g_device = device;
	g_deviceHooks = true;
	BootMark("D3D9", "EndScene+Reset hooked");
	SFC_LOG("[D3D9] Present draw reverted — EndScene only (v5 Present crashed NVSE)");
	return true;
}

HRESULT STDMETHODCALLTYPE hkCreateDevice(
	IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags,
	D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** outDevice)
{
	BootMark("D3D9", "CreateDevice");
	const HRESULT hr = oCreateDevice(self, adapter, type, hwnd, flags, pp, outDevice);
	if (SUCCEEDED(hr) && outDevice && *outDevice) {
		HookDevice(*outDevice);
	}
	return hr;
}

bool HookD3D9Object(IDirect3D9* d3d)
{
	if (!d3d || g_createHooks) return g_createHooks;
	void** vtable = *reinterpret_cast<void***>(d3d);
	if (MH_CreateHook(vtable[16], &hkCreateDevice, reinterpret_cast<void**>(&oCreateDevice)) != MH_OK) {
		return false;
	}
	if (MH_EnableHook(vtable[16]) != MH_OK) return false;
	g_createHooks = true;
	BootMark("D3D9", "CreateDevice hooked");
	return true;
}

IDirect3D9* WINAPI hkDirect3DCreate9(UINT sdk)
{
	BootMark("D3D9", "Direct3DCreate9");
	IDirect3D9* d3d = oDirect3DCreate9(sdk);
	if (d3d) HookD3D9Object(d3d);
	return d3d;
}

bool HookDirect3DCreate9Export()
{
	HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
	if (!d3d9) d3d9 = LoadLibraryA("d3d9.dll");
	if (!d3d9) return false;

	auto* create = reinterpret_cast<Direct3DCreate9_t>(GetProcAddress(d3d9, "Direct3DCreate9"));
	if (!create) return false;

	if (MH_CreateHook(reinterpret_cast<void*>(create), &hkDirect3DCreate9, reinterpret_cast<void**>(&oDirect3DCreate9)) != MH_OK)
		return false;
	if (MH_EnableHook(reinterpret_cast<void*>(create)) != MH_OK)
		return false;

	BootMark("D3D9", "Direct3DCreate9 export hooked");
	return true;
}

DWORD WINAPI LateHookThread(LPVOID)
{
	for (int i = 0; i < 40 && !g_deviceHooks && !g_shutdown; ++i) {
		Sleep(250);
		if (g_deviceHooks) return 0;
	}
	if (!g_deviceHooks)
		SFC_WARN("[D3D9] Device hooks not installed yet — waiting for CreateDevice");
	return 0;
}

} // namespace

IDirect3DDevice9* GetD3D9Device() { return g_device; }

void HardDisableOverlayDraws(const char* reason)
{
	g_hardDisableDraw = true;
	SFC_ERR("[HUD] HARD DISABLE overlay draws: %s", reason ? reason : "unknown");
}

bool InstallD3D9Hooks()
{
	g_shutdown = false;
	g_hardDisableDraw = false;
	g_rendering = false;
	g_inReset = false;
	MH_Initialize();

	const bool ok = HookDirect3DCreate9Export();
	if (!ok) {
		SFC_ERR("[D3D9] Failed to hook Direct3DCreate9");
		return false;
	}

	HANDLE h = CreateThread(nullptr, 0, LateHookThread, nullptr, 0, nullptr);
	if (h) CloseHandle(h);
	return true;
}

void ShutdownD3D9Hooks()
{
	g_shutdown = true;
	g_inReset = true;
	ReleaseStateBlock();
	MH_DisableHook(MH_ALL_HOOKS);
	g_deviceHooks = false;
	g_createHooks = false;
	g_device = nullptr;
	BootMark("SHUTDOWN", "D3D hooks disabled");
}

} // namespace sfc
