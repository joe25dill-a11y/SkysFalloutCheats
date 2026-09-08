#include "core/ConsoleBridge.hpp"
#include "core/GameWorkQueue.hpp"
#include "core/Log.hpp"
#include <cstdarg>
#include <cstdio>
#include <windows.h>

namespace sfc {
namespace {

constexpr std::uintptr_t kPreferredBase = 0x00400000;
constexpr std::uintptr_t kFormsMapAbs = 0x011C54C0;

std::uintptr_t Rel(std::uintptr_t absAddr)
{
	const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
	return base + (absAddr - kPreferredBase);
}

} // namespace

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

void* ConsoleBridge::LookupRef(std::uint32_t refId) const
{
	if (refId == 0) return nullptr;
	__try {
		struct FormsMap {
			void** vtbl;
			std::uint32_t numBuckets;
			struct Entry {
				Entry* next;
				std::uint32_t key;
				void* data;
			}** buckets;
			std::uint32_t numItems;
		};
		FormsMap** slot = reinterpret_cast<FormsMap**>(Rel(kFormsMapAbs));
		FormsMap* map = slot ? *slot : nullptr;
		if (!map || !map->buckets || map->numBuckets == 0) return nullptr;
		for (auto* e = map->buckets[refId % map->numBuckets]; e; e = e->next) {
			if (e->key == refId) return e->data;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
	return nullptr;
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

bool ConsoleBridge::RunImmediateOnRef(std::uint32_t refId, const std::string& cmd)
{
	if (!IsReady()) {
		SFC_WARN("Console OnRef skipped (unavailable): %08X -> %s", refId, cmd.c_str());
		return false;
	}
	void* refr = LookupRef(refId);
	if (!refr) {
		SFC_WARN("Console OnRef: ref %08X not found for: %s", refId, cmd.c_str());
		return false;
	}
	const bool ok = console_->RunScriptLine2(cmd.c_str(), reinterpret_cast<TESObjectREFR*>(refr), true);
	if (!ok) {
		// Fallback: unquoted ref prefix (some builds accept this when callingRef path fails).
		char alt[768];
		std::snprintf(alt, sizeof(alt), "%08X.%s", refId, cmd.c_str());
		const bool ok2 = console_->RunScriptLine2(alt, nullptr, true);
		if (!ok2) {
			SFC_WARN("Console OnRef failed: %08X -> %s", refId, cmd.c_str());
			return false;
		}
		SFC_LOG("Console OnRef ok via prefix: %s", alt);
		return true;
	}
	return true;
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

bool ConsoleBridge::RunOnRef(std::uint32_t refId, const char* cmd)
{
	if (!cmd || !*cmd) return false;
	if (!IsReady()) {
		SFC_WARN("Console OnRef skipped (unavailable): %08X -> %s", refId, cmd);
		return false;
	}
	if (GameWorkQueue::Get().ShouldDeferConsole())
		return GameWorkQueue::Get().EnqueueConsoleOnRef(refId, cmd);
	return RunImmediateOnRef(refId, cmd);
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

bool ConsoleBridge::RunOnReff(std::uint32_t refId, const char* fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	std::vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	return RunOnRef(refId, buf);
}

} // namespace sfc
