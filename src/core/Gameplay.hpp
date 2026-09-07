#pragma once

namespace sfc {

bool HasPlayer();
bool CanDrawOverlay();
bool IsLoadingScreen();
bool IsGameMenuBlocking();

// Updated on MainGameLoop — EndScene should prefer this over live CanDrawOverlay().
void RefreshOverlayGateCache();
bool OverlayGateCached();

} // namespace sfc
