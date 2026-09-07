#pragma once
#include "PluginAPI_Lean.h"

namespace sfc {

// Register NVSE MainGameLoop (+ load/exit) so game-world work is not on EndScene.
bool InstallNvseMessages(PluginHandle handle, NVSEInterface* nvse);

} // namespace sfc
