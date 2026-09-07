#include "core/Diag.hpp"
#include "core/Config.hpp"
#include "core/Log.hpp"
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace sfc {
namespace {

std::mutex g_mutex;
std::uint64_t g_seq = 0;
bool g_override = false;
bool g_overrideOn = false;

struct ThrottleEntry {
	unsigned lastMs = 0;
};

std::unordered_map<std::string, ThrottleEntry> g_throttle;

} // namespace

bool DiagEnabled()
{
	if (g_override) return g_overrideOn;
	return Config::Get().Data().diagnostics.enabled;
}

void DiagSetEnabled(bool on)
{
	g_override = true;
	g_overrideOn = on;
}

std::uint64_t DiagNextSeq()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return ++g_seq;
}

void DiagEvent(const char* op, const char* detail)
{
	if (!DiagEnabled() || !op) return;
	const auto seq = DiagNextSeq();
	if (detail && *detail)
		SFC_LOG("[DIAG] seq=%llu op=%s event detail=%s", static_cast<unsigned long long>(seq), op, detail);
	else
		SFC_LOG("[DIAG] seq=%llu op=%s event", static_cast<unsigned long long>(seq), op);
}

void DiagBegin(std::uint64_t seq, const char* op, const char* detail)
{
	if (!DiagEnabled() || !op) return;
	if (detail && *detail)
		SFC_LOG("[DIAG] seq=%llu op=%s start detail=%s", static_cast<unsigned long long>(seq), op, detail);
	else
		SFC_LOG("[DIAG] seq=%llu op=%s start", static_cast<unsigned long long>(seq), op);
}

void DiagEnd(std::uint64_t seq, const char* op, bool ok, unsigned durationMs, const char* detail)
{
	if (!DiagEnabled() || !op) return;
	if (detail && *detail)
		SFC_LOG("[DIAG] seq=%llu op=%s %s ms=%u detail=%s",
			static_cast<unsigned long long>(seq), op, ok ? "ok" : "fail", durationMs, detail);
	else
		SFC_LOG("[DIAG] seq=%llu op=%s %s ms=%u",
			static_cast<unsigned long long>(seq), op, ok ? "ok" : "fail", durationMs);
}

void DiagThrottle(const char* key, unsigned minIntervalMs, const char* fmt, ...)
{
	if (!DiagEnabled() || !key || !fmt) return;
	const unsigned now = GetTickCount();
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		auto& e = g_throttle[key];
		if (e.lastMs != 0 && (now - e.lastMs) < minIntervalMs)
			return;
		e.lastMs = now;
	}

	char body[1024];
	va_list args;
	va_start(args, fmt);
	std::vsnprintf(body, sizeof(body), fmt, args);
	va_end(args);

	const auto seq = DiagNextSeq();
	SFC_LOG("[DIAG] seq=%llu op=%s throttle %s", static_cast<unsigned long long>(seq), key, body);
}

DiagScope::DiagScope(const char* op, const char* detail)
	: op_(op)
{
	if (!DiagEnabled() || !op_) return;
	finished_ = false;
	seq_ = DiagNextSeq();
	startMs_ = GetTickCount();
	DiagBegin(seq_, op_, detail);
}

DiagScope::~DiagScope()
{
	if (finished_ || !op_) return;
	DiagEnd(seq_, op_, ok_, GetTickCount() - startMs_, nullptr);
}

void DiagScope::Fail(const char* detail)
{
	if (finished_ || !op_) return;
	ok_ = false;
	DiagEnd(seq_, op_, false, GetTickCount() - startMs_, detail);
	finished_ = true;
}

void DiagScope::Ok(const char* detail)
{
	if (finished_ || !op_) return;
	ok_ = true;
	DiagEnd(seq_, op_, true, GetTickCount() - startMs_, detail);
	finished_ = true;
}

} // namespace sfc
