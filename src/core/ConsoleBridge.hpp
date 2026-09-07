#pragma once
#include "PluginAPI_Lean.h"
#include <string>

namespace sfc {

class ConsoleBridge {
public:
	static ConsoleBridge& Get();

	void SetInterface(NVSEConsoleInterface* console);
	bool IsReady() const { return console_ != nullptr && console_->RunScriptLine2 != nullptr; }

	// Queues when outside MainGameLoop; executes immediately when draining in-loop.
	bool Run(const std::string& line);
	bool Runf(const char* fmt, ...);

	// Always RunScriptLine2 now (used by GameWorkQueue drain).
	bool RunImmediate(const std::string& line);

private:
	NVSEConsoleInterface* console_ = nullptr;
};

} // namespace sfc
