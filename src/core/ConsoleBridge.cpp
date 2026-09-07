#include "core/ConsoleBridge.hpp"
#include "core/GameWorkQueue.hpp"
#include "core/Log.hpp"
#include <cstdarg>
#include <cstdio>

namespace sfc {

ConsoleBridge& ConsoleBridge::Get()
{
	static ConsoleBridge instance;
	return instance;
}

void ConsoleBridge::SetInterface(NVSEConsoleInterface* console)
{
	console_ = console;
	if (IsReady()) SFC_LOG("Console bridge ready (NVSEConsoleInterface v%u)", console_->version);
	else SFC_WARN("Console bridge unavailable");
}

bool ConsoleBridge::RunImmediate(const std::string& line)
{
	if (!IsReady()) {
		SFC_WARN("Console command skipped (unavailable): %s", line.c_str());
		return false;
	}
	const bool ok = console_->RunScriptLine2(line.c_str(), nullptr, true);
	if (!ok) SFC_WARN("Console command failed: %s", line.c_str());
	return ok;
}

bool ConsoleBridge::Run(const std::string& line)
{
	if (!IsReady()) {
		SFC_WARN("Console command skipped (unavailable): %s", line.c_str());
		return false;
	}
	if (GameWorkQueue::Get().ShouldDeferConsole())
		return GameWorkQueue::Get().EnqueueConsole(line);
	return RunImmediate(line);
}

bool ConsoleBridge::Runf(const char* fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	std::vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	return Run(buf);
}

} // namespace sfc
