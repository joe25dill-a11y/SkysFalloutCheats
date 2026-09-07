#pragma once
#include <d3d9.h>

namespace sfc {

bool InstallD3D9Hooks();
void ShutdownD3D9Hooks();
void HardDisableOverlayDraws(const char* reason);
IDirect3DDevice9* GetD3D9Device();

} // namespace sfc
