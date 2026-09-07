#include "core/GameWorkQueue.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/Diag.hpp"
#include "core/Input.hpp"
#include "core/Boot.hpp"
#include "core/Log.hpp"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cctype>

namespace sfc {
namespace {

bool ContainsInsensitive(const char* hay, const char* needle)
{
	if (!hay || !needle || !*needle) return false;
	for (const char* h = hay; *h; ++h) {
		const char* a = h;
		const char* b = needle;
		while (*a && *b) {
			const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(*a)));
			const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(*b)));
			if (ca != cb) break;
			++a;
			++b;
		}
		if (!*b) return true;
	}
	return false;
}

bool IsHeavyConsoleLine(const char* line)
{
	// These rebuild actors / cells / AI — unsafe to spam, especially with ImGui open.
	// Do NOT match bare "kill" as a substring — it hits "skills".
	return ContainsInsensitive(line, "resurrect")
		|| ContainsInsensitive(line, "placeatme")
		|| ContainsInsensitive(line, "moveto")
		|| ContainsInsensitive(line, "resetai")
		|| ContainsInsensitive(line, "coc ")
		|| (std::strncmp(line, "coc ", 4) == 0)
		|| ContainsInsensitive(line, "kah")
		|| ContainsInsensitive(line, "kill ")
		|| ContainsInsensitive(line, " kill")
		|| ContainsInsensitive(line, ".kill")
		|| (_stricmp(line, "kill") == 0)
		|| (_stricmp(line, "ka") == 0);
}

} // namespace

GameWorkQueue& GameWorkQueue::Get()
{
	static GameWorkQueue instance;
	return instance;
}

void GameWorkQueue::SetGameLoopAvailable(bool available)
{
	gameLoopAvailable_ = available;
	SFC_LOG("[GAMEWORK] MainGameLoop %s", available ? "ACTIVE (preferred)" : "UNAVAILABLE (EndScene fallback)");
}

void GameWorkQueue::EnterGameLoop() { inGameLoop_ = true; }
void GameWorkQueue::LeaveGameLoop() { inGameLoop_ = false; }
void GameWorkQueue::EnterRender() { inRender_ = true; }
void GameWorkQueue::LeaveRender() { inRender_ = false; }

bool GameWorkQueue::ShouldDeferConsole() const
{
	return !inGameLoop_;
}

bool GameWorkQueue::EnqueueConsole(const std::string& line)
{
	if (line.empty()) return false;

	// De-dupe identical pending lines (ImGui Button stays true while held → spam).
	for (int i = 0; i < count_; ++i) {
		const int idx = (head_ + i) % kMaxPending;
		if (std::strcmp(lines_[idx], line.c_str()) == 0)
			return true;
	}

	if (count_ >= kMaxPending) {
		++dropped_;
		SFC_WARN("[GAMEWORK] console queue full — dropped: %s (totalDropped=%u)", line.c_str(), dropped_);
		return false;
	}

	const bool heavy = IsHeavyConsoleLine(line.c_str());
	const unsigned now = GetTickCount();
	unsigned delay = 0;
	if (heavy) {
		delay = 400;
		// Close INSERT so we are not mid-ImGui when resurrect rebuilds the player.
		if (Input::Get().MenuOpen()) Input::Get().SetMenuOpen(false);
		if (Input::Get().SearchOpen()) Input::Get().SetSearchOpen(false);
		SFC_LOG("[GAMEWORK] heavy cmd queued (+%ums, menu closed): %s", delay, line.c_str());
	}

	const int idx = (head_ + count_) % kMaxPending;
	std::snprintf(lines_[idx], sizeof(lines_[idx]), "%s", line.c_str());
	fireAtMs_[idx] = now + delay;
	++count_;
	if (DiagEnabled())
		DiagEvent("console.enqueue", line.c_str());
	return true;
}

void GameWorkQueue::DrainConsole()
{
	const unsigned now = GetTickCount();
	while (count_ > 0) {
		if (now < fireAtMs_[head_])
			break; // FIFO — wait for delay

		const char* line = lines_[head_];
		const bool heavy = IsHeavyConsoleLine(line);

		if (heavy && lastHeavyRunMs_ != 0 && (now - lastHeavyRunMs_) < 1500) {
			SFC_WARN("[GAMEWORK] heavy cmd rate-limited (dropped): %s", line);
			head_ = (head_ + 1) % kMaxPending;
			--count_;
			continue;
		}

		DiagScope scope("console.run", line);
		const bool ok = ConsoleBridge::Get().RunImmediate(line);
		if (heavy) {
			lastHeavyRunMs_ = GetTickCount();
			// Give Fallout time to rebuild actor/HUD meshes — stop our overlay fighting it.
			ResetWorldSettle("heavy_console");
		}
		if (!ok) scope.Fail("RunScriptLine2 failed");
		else scope.Ok();

		head_ = (head_ + 1) % kMaxPending;
		--count_;
	}
}

std::size_t GameWorkQueue::PendingConsole() const
{
	return static_cast<std::size_t>(count_);
}

} // namespace sfc
