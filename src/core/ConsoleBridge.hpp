#pragma once
#include "PluginAPI_Lean.h"
#include <cstdint>
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

	// Run a command AS a reference (correct for Activate / RemoveAllItems / etc.).
	// Prefer this over "\"%08X\".cmd" strings — those fail RunScriptLine2 in practice.
	bool RunOnRef(std::uint32_t refId, const char* cmd);
	bool RunOnReff(std::uint32_t refId, const char* fmt, ...);

	bool RunImmediate(const std::string& line);
	bool RunImmediateOnRef(std::uint32_t refId, const std::string& cmd);

private:
	void* LookupRef(std::uint32_t refId) const;

	NVSEConsoleInterface* console_ = nullptr;
};

} // namespace sfc
