#include "PluginAPI_Lean.h"
#include "core/Log.hpp"
#include "core/Boot.hpp"
#include "core/App.hpp"
#include "core/Compat.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/NvseMessages.hpp"
#include "render/D3D9Hook.hpp"
#include <windows.h>
#include <string>

namespace {
PluginHandle g_pluginHandle = kPluginHandle_Invalid;
NVSEInterface* g_nvse = nullptr;
bool g_started = false;
}

extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info)
{
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "SkysFalloutCheats";
	info->version = 1;

	if (nvse->isEditor) return false;
	if (!nvse->nvseVersion) return false;
	return true;
}

extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface* nvse)
{
	g_nvse = const_cast<NVSEInterface*>(nvse);
	g_pluginHandle = nvse->GetPluginHandle();

	const char* runtimeDir = nvse->GetRuntimeDirectory ? nvse->GetRuntimeDirectory() : nullptr;
	std::string dataDir = runtimeDir ? (std::string(runtimeDir) + "Data\\NVSE\\Plugins\\SkysFalloutCheats") : "SkysFalloutCheats";
	CreateDirectoryA((std::string(runtimeDir ? runtimeDir : "") + "Data\\NVSE\\Plugins").c_str(), nullptr);
	CreateDirectoryA(dataDir.c_str(), nullptr);

	sfc::LogInit(dataDir + "\\sfc.log");
	sfc::BootMark("NVSE", "NVSEPlugin_Load");
	SFC_LOG("[NVSE] nvse=0x%X runtime=0x%X", nvse->nvseVersion, nvse->runtimeVersion);

	auto* console = static_cast<NVSEConsoleInterface*>(nvse->QueryInterface(kInterface_Console));
	sfc::ConsoleBridge::Get().SetInterface(console);

	sfc::Compat().nvseVersion = nvse->nvseVersion;
	sfc::Compat().runtimeVersion = nvse->runtimeVersion;
	sfc::CompatProbe(runtimeDir, sfc::ConsoleBridge::Get().IsReady(), nvse->nvseVersion, nvse->runtimeVersion);

	if (!sfc::App::Get().Init(runtimeDir)) {
		SFC_ERR("[BOOT] App init failed");
		return false;
	}

	sfc::BootMark("NVSE", "register messaging / MainGameLoop");
	sfc::InstallNvseMessages(g_pluginHandle, g_nvse);

	sfc::BootMark("D3D9", "install hooks");
	if (!sfc::InstallD3D9Hooks()) {
		SFC_WARN("[D3D9] Initial hook incomplete — retry thread will continue");
	}

	g_started = true;
	sfc::BootMark("BOOT", "NVSEPlugin_Load complete — idle until world settle");
	return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hModule);
	} else if (reason == DLL_PROCESS_DETACH) {
		// Loader-lock safe: only disable hooks / flags. No ImGui destroy, no MH_Uninitialize.
		if (g_started) {
			sfc::ShutdownD3D9Hooks();
			g_started = false;
		}
	}
	return TRUE;
}
