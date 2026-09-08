#pragma once
#include <d3d9.h>

namespace sfc {

struct ScreenPos {
	float x = 0.f;
	float y = 0.f;
	bool ok = false;
};

// Call once per frame in EndScene BEFORE ImGui touches the device.
void CaptureCameraForFrame(IDirect3DDevice9* device);

ScreenPos WorldToScreen(float wx, float wy, float wz);

bool WorldBoxToScreenRect(
	float cx, float cy, float cz,
	float halfW, float halfH, float halfD,
	float& outMinX, float& outMinY, float& outMaxX, float& outMaxY);

} // namespace sfc
