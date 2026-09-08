#pragma once
#include <d3d9.h>

namespace sfc {

struct ScreenPos {
	float x = 0.f;
	float y = 0.f;
	bool ok = false;     // in front of camera with a usable projection
	bool behind = false; // world point is behind the look plane
};

// Call once per frame in EndScene BEFORE ImGui touches the device.
void CaptureCameraForFrame(IDirect3DDevice9* device);

ScreenPos WorldToScreen(float wx, float wy, float wz);

// Always produces a screen point for ESP:
// - on-screen when in front and inside the view
// - clamped to the screen edge when off to the side / behind (wallhack pings)
// Returns false only if the camera frame is invalid.
bool ProjectEspPoint(float wx, float wy, float wz, float& sx, float& sy, bool& onScreen);

bool WorldBoxToScreenRect(
	float cx, float cy, float cz,
	float halfW, float halfH, float halfD,
	float& outMinX, float& outMinY, float& outMaxX, float& outMaxY);

} // namespace sfc
