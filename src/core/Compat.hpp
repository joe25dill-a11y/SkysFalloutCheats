#pragma once
#include <string>

namespace sfc {

struct CompatState {
	bool nvseOk = false;
	bool consoleOk = false;
	bool jipPresent = false;
	bool johnnyPresent = false;
	std::string runtimeDir;
	std::string pluginDataDir;
	unsigned int nvseVersion = 0;
	unsigned int runtimeVersion = 0;
};

CompatState& Compat();
void CompatProbe(const char* runtimeDir, bool consoleOk, unsigned int nvseVersion, unsigned int runtimeVersion);
bool FileExistsNearPlugins(const char* dllName);

} // namespace sfc
