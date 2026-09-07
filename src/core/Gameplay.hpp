#pragma once

namespace sfc {

// Player pointer exists (save loaded / in session).
bool HasPlayer();

// Safe to draw ImGui overlay: in-world, settled, not loading, not in Pip-Boy/etc.
bool CanDrawOverlay();

// True while the game LoadingMenu is up (cell transitions, save load, etc.).
bool IsLoadingScreen();

// Pip-Boy inventory/stats/map, containers, dialog, pause, etc. — do not draw over these.
bool IsGameMenuBlocking();

} // namespace sfc
