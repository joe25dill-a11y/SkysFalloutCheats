#include "core/Compat.hpp"
#include "core/Log.hpp"
#include <windows.h>
#include <cstdio>

namespace sfc {
namespace {
CompatState g_compat;
}

CompatState& Compat() { return g_compat; }

bool FileExistsNearPlugins(const char* dllName)
{
	char path[MAX_PATH]{};
	if (!g_compat.runtimeDir.empty()) {
		std::snprintf(path, sizeof(path), "%s\\Data\\NVSE\\Plugins\\%s", g_compat.runtimeDir.c_str(), dllName);
		DWORD attr = GetFileAttributesA(path);
		if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) return true;
	}
	return GetModuleHandleA(dllName) != nullptr;
}

void CompatProbe(const char* runtimeDir, bool consoleOk, unsigned int nvseVersion, unsigned int runtimeVersion)
{
	g_compat.nvseOk = true;
	g_compat.consoleOk = consoleOk;
	g_compat.nvseVersion = nvseVersion;
	g_compat.runtimeVersion = runtimeVersion;
	g_compat.runtimeDir = runtimeDir ? runtimeDir : "";
	if (!g_compat.runtimeDir.empty()) {
		g_compat.pluginDataDir = g_compat.runtimeDir + "\\Data\\NVSE\\Plugins\\SkysFalloutCheats";
	}
	g_compat.jipPresent = FileExistsNearPlugins("jip_nvse.dll") || FileExistsNearPlugins("JIP_NVSE.dll");
	g_compat.johnnyPresent = FileExistsNearPlugins("johnnyguitar.dll") || FileExistsNearPlugins("JohnnyGuitarNVSE.dll");

	SFC_LOG("Compat: NVSE=%u runtime=%u console=%d JIP=%d Johnny=%d",
		nvseVersion, runtimeVersion, consoleOk ? 1 : 0,
		g_compat.jipPresent ? 1 : 0, g_compat.johnnyPresent ? 1 : 0);
	SFC_LOG("Plugin data dir: %s", g_compat.pluginDataDir.c_str());
}

} // namespace sfc
