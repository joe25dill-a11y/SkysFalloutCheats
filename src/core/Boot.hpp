#pragma once

namespace sfc {

// Staged boot milestones — search sfc.log for these tags during freezes/crashes.
void BootMark(const char* stage, const char* detail = nullptr);

// Called from EndScene gate each frame.
// Returns true only after player+cell are stable for several consecutive frames
// AND loading menu is down. Resets immediately when loading or player/cell vanish.
bool ObserveWorldReady();

// True once ObserveWorldReady has fully armed at least once this session.
bool WorldEverReady();

// Clears settle counters (loading / leaving world).
void ResetWorldSettle(const char* reason);

} // namespace sfc
