#include "core/App.hpp"
#include "core/Log.hpp"
#include "core/Boot.hpp"
#include "core/Config.hpp"
#include "core/Compat.hpp"
#include "core/FeatureRegistry.hpp"
#include "core/Input.hpp"
#include "core/Hotkeys.hpp"
#include "core/SearchIndex.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/Cheats.hpp"
#include "core/GameState.hpp"
#include "core/Gameplay.hpp"
#include "core/CompanionFollow.hpp"
#include "core/Diag.hpp"
#include "core/GameWorkQueue.hpp"
#include "core/Isolation.hpp"
#include "ui/Theme.hpp"
#include "ui/MainMenu.hpp"
#include "ui/Hud.hpp"
#include "ui/SearchOverlay.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <filesystem>
#include <windows.h>

namespace sfc {

App& App::Get()
{
	static App instance;
	return instance;
}

bool App::Init(const char* runtimeDir)
{
	if (ready_) return true;

	std::filesystem::path dataDir;
	if (runtimeDir && *runtimeDir) {
		dataDir = std::filesystem::path(runtimeDir) / "Data" / "NVSE" / "Plugins" / "SkysFalloutCheats";
	} else {
		dataDir = "SkysFalloutCheats";
	}
	std::error_code ec;
	std::filesystem::create_directories(dataDir, ec);

	LogInit((dataDir / "sfc.log").string());
	BootMark("BOOT", "App::Init begin");
	SFC_LOG("[BOOT] build=%s %s playable-v17j (ESP 8-corner + live FOV, no angular)", __DATE__, __TIME__);

	BootMark("NVSE", ConsoleBridge::Get().IsReady() ? "console ready" : "console UNAVAILABLE");
	CompatProbe(runtimeDir, ConsoleBridge::Get().IsReady(), Compat().nvseVersion, Compat().runtimeVersion);

	BootMark("CONFIG", "loading");
	auto& cfg = Config::Get();
	cfg.SetPath(dataDir / "config.json");
	cfg.Load(cfg.Path());
	cfg.Data().theme.crtEffects = false;
	cfg.Data().theme.scanlines = false;
	cfg.Data().theme.glow = false;
	// ESP stays opt-in (off by default). Do not force-disable every boot — that
	// made the ESP tab look permanently broken after the user enabled it.
	if (cfg.Data().performance.espEnabled) {
		SFC_WARN("[CONFIG] ESP enabled — experimental (world scan + W2S)");
	}
	cfg.Data().diagnostics.enabled = true;
	// Live stats come from vanilla HUD tiles (same as GetUIFloat), NOT ActorValueOwner.
	cfg.Data().diagnostics.isolationMode = "off";
	cfg.Data().hud.liveHud = true;
	cfg.Data().hud.enabled = true;
	cfg.Data().hud.statusBar = true;
	cfg.Data().hud.combatInfo = true;
	cfg.Data().hud.worldInfo = true;
	// Do NOT MarkDirty here — that forced a disk save mid-load and correlated with freezes.
	IsolationLogBoot();
	SFC_LOG("[CONFIG] isolation=off — playable-v17 nearby scan / ESP");
	BootMark("CONFIG", "loaded");
	ResetWorldSettle("boot");

	BootMark("FEATURES", "register+init (no game-memory reads expected)");
	RegisterBuiltinFeatures();
	FeatureRegistry::Get().InitAll();
	SearchIndex::Get().Rebuild();
	BootMark("FEATURES", "ready");

	Hotkeys::Get().Bind({"toggle_menu", "Toggle Menu", cfg.Data().controls.menuToggleVk, false, false, false, true});
	Hotkeys::Get().Bind({"toggle_search", "Command Search", cfg.Data().controls.searchToggleVk, false, false, false, true});
	Hotkeys::Get().Bind({"god_mode", "God Mode", cfg.Data().controls.godModeVk, false, false, false, true});
	Hotkeys::Get().Bind({"full_heal", "Full Heal", cfg.Data().controls.healVk, false, false, false, true});
	Hotkeys::Get().Bind({"add_caps", "Add Caps", cfg.Data().controls.addCapsVk, false, false, false, true});

	Hotkeys::Get().SetCallback("god_mode", []() {
		if (!IsolationAllowHotkeys() || !IsolationAllowConsole()) return;
		if (!ConsoleBridge::Get().IsReady()) return;
		CheatToggleGodMode();
	});
	Hotkeys::Get().SetCallback("full_heal", []() {
		if (!IsolationAllowHotkeys() || !IsolationAllowConsole()) return;
		if (!ConsoleBridge::Get().IsReady()) return;
		CheatFullHeal();
	});
	Hotkeys::Get().SetCallback("add_caps", []() {
		if (!IsolationAllowHotkeys() || !IsolationAllowConsole()) return;
		if (!ConsoleBridge::Get().IsReady()) return;
		CheatAddCaps(1000);
	});

	// Prove which DLL file is actually loaded (stale-deploy trap).
	char modPath[MAX_PATH]{};
	HMODULE self = GetModuleHandleA("SkysFalloutCheats.dll");
	if (self && GetModuleFileNameA(self, modPath, MAX_PATH)) {
		WIN32_FILE_ATTRIBUTE_DATA fad{};
		if (GetFileAttributesExA(modPath, GetFileExInfoStandard, &fad)) {
			SYSTEMTIME st{};
			FileTimeToSystemTime(&fad.ftLastWriteTime, &st);
			SFC_LOG("[BOOT] dll=%s mtime=%04u-%02u-%02u %02u:%02u:%02u UTC",
				modPath, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		} else {
			SFC_LOG("[BOOT] dll=%s", modPath);
		}
	}

	ready_ = true;
	BootMark("BOOT", "App ready — waiting for [GAME_READY] before HUD");
	return true;
}

void App::Shutdown()
{
	if (!ready_) return;
	BootMark("SHUTDOWN", "App::Shutdown");
	Config::Get().SaveIfDirty();
	FeatureRegistry::Get().ShutdownAll();
	LogShutdown();
	ready_ = false;
}

void App::TickGameWorld()
{
	if (!ready_) return;
	if (IsLoadingScreen()) return;
	if (IsolationStaticImGuiOnly()) return;

	auto& q = GameWorkQueue::Get();
	q.EnterGameLoop();

	// Gate + GameState live here — NOT in EndScene.
	static int gateDiv = 0;
	if ((++gateDiv % 8) == 0) // ~every 8 game loops, not every frame
		RefreshOverlayGateCache();
	else if (!OverlayGateCached() && ObserveWorldReady())
		RefreshOverlayGateCache(); // retry sooner while waiting to arm

	DiagThrottle("game.loop", 5000, "TickGameWorld pendingConsole=%zu gate=%d",
		q.PendingConsole(), OverlayGateCached() ? 1 : 0);

	if (IsolationAllowConsole())
		q.DrainConsole();

	if (ObserveWorldReady() && !IsGameMenuBlocking())
		TickHudReads();

	if (IsolationAllowCompanionAndTeleportTicks()) {
		CompanionFollow::Get().Tick();
		FeatureRegistry::Get().TickAll(1.f / 60.f);
	}

	q.LeaveGameLoop();
}

void App::TickHudReads()
{
	if (!ready_) return;
	if (!IsolationAllowGameStateReads()) return;
	if (IsLoadingScreen() || IsGameMenuBlocking()) return;
	if (!ObserveWorldReady()) return;
	GameState::Get().Tick(1.f / 60.f);
}

void App::OnFrame()
{
	if (!ready_) return;

	float dt = ImGui::GetIO().DeltaTime;
	if (dt <= 0.f || dt > 0.5f) dt = 1.f / 60.f;

	DiagThrottle("render.frame", 5000, "OnFrame isolation=%s menu=%d",
		IsolationName(IsolationGet()), Input::Get().MenuOpen() ? 1 : 0);

	static ThemeSettings lastTheme{};
	static bool themeInit = false;
	auto& theme = Config::Get().Data().theme;
	if (!themeInit ||
		theme.uiScale != lastTheme.uiScale ||
		theme.opacity != lastTheme.opacity ||
		theme.accentR != lastTheme.accentR ||
		theme.accentG != lastTheme.accentG ||
		theme.accentB != lastTheme.accentB ||
		theme.fontSize != lastTheme.fontSize) {
		Theme::Apply(theme);
		lastTheme = theme;
		themeInit = true;
	}

	if (IsolationStaticImGuiOnly()) {
		// TEST A — static window only. No GameState, no menu, no cheats.
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;
		ImGui::SetNextWindowPos(ImVec2(16.f, 16.f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.65f);
		if (ImGui::Begin("##SFC_DIAG_STATIC", nullptr, flags)) {
			ImGui::TextColored(ImVec4(1.f, 0.72f, 0.2f, 1.f), "SFC DIAG");
			ImGui::Text("static_imgui isolation");
			ImGui::TextDisabled("No GameState / console / companions");
			ImGui::TextDisabled("If Fallout HUD still glitches → D3D/ImGui");
		}
		ImGui::End();
	} else {
		Hud::Draw();
		// ESP side list / boxes draw even with menu open (list is the reliable path).
		if (!Input::Get().SearchOpen()) {
			FeatureRegistry::Get().DrawHudAll();
		}
		if (IsolationAllowMainMenu()) {
			MainMenu::Draw();
			SearchOverlay::Draw();
		}
		DrawNotifications();
	}

	static float saveAccum = 0.f;
	saveAccum += dt;
	if (saveAccum >= 2.0f) {
		saveAccum = 0.f;
		Config::Get().SaveIfDirty();
	}
}

} // namespace sfc
