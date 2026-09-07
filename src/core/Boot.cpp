#include "core/Boot.hpp"
#include "core/Log.hpp"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <windows.h>

namespace sfc {
namespace {

std::atomic<unsigned> g_readyAfterMs{0};
std::atomic<bool> g_everReady{false};
char g_lastStage[32]{};
constexpr unsigned kSettleMs = 4000; // real time — EndScene frame counts race through doors

unsigned NowMs()
{
	return GetTickCount();
}

} // namespace

void BootMark(const char* stage, const char* detail)
{
	if (!stage) return;
	if (std::strcmp(g_lastStage, stage) == 0 && (!detail || !detail[0]))
		return;
	std::snprintf(g_lastStage, sizeof(g_lastStage), "%s", stage);
	if (detail && detail[0])
		SFC_LOG("[%s] %s", stage, detail);
	else
		SFC_LOG("[%s]", stage);
}

void ResetWorldSettle(const char* reason)
{
	const unsigned now = NowMs();
	const unsigned readyAt = now + kSettleMs;
	const unsigned prev = g_readyAfterMs.exchange(readyAt);
	// Only log when we actually push the timer out (edge), not every frame.
	if (reason && (prev == 0 || prev <= now + 500))
		SFC_LOG("[GAME_READY] settle reset (%s) — no overlay for %ums", reason, kSettleMs);
}

bool ObserveWorldReady()
{
	const unsigned readyAt = g_readyAfterMs.load();
	const unsigned now = NowMs();

	// First candidate after a reset: readyAt was set in ResetWorldSettle.
	// Fresh session: readyAt==0 → treat as needing a settle window from now.
	if (readyAt == 0) {
		g_readyAfterMs.store(now + kSettleMs);
		BootMark("GAME_READY", "world candidate — settling (timer)");
		return false;
	}

	if (now < readyAt) {
		return false;
	}

	if (!g_everReady.exchange(true))
		BootMark("GAME_READY", "overlay armed (settle timer elapsed)");
	return true;
}

bool WorldEverReady()
{
	return g_everReady.load();
}

} // namespace sfc
