#pragma once
#include <d3d9.h>

namespace sfc {

class ImGuiBackend {
public:
	static bool Init(IDirect3DDevice9* device);
	static void Shutdown();
	static void OnEndScene(IDirect3DDevice9* device);
	static void Render();
	static void OnLostDevice();
	static void OnResetDevice();
	static bool Ready();
};

} // namespace sfc
