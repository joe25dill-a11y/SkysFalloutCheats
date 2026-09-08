#pragma once
#include <d3d9.h>

namespace sfc {

struct ScreenPos {
	float x = 0.f;
	float y = 0.f;
	bool ok = false;
};

struct EspCamInfo {
	bool valid = false;
	int source = 0;       // 5/6 player-look, 9+ FOV from live source
	int fovSource = 0;    // 0=default75, 1=d3d, 2=worldSG, 3=ifaceSG
	float sw = 0, sh = 0;
	float eyeX = 0, eyeY = 0, eyeZ = 0;
	float yawDeg = 0, pitchDeg = 0, fovDeg = 0;
};

// Call once per frame in EndScene BEFORE ImGui touches the device.
void CaptureCameraForFrame(IDirect3DDevice9* device);

bool GetEspCamInfo(EspCamInfo& out);

ScreenPos WorldToScreen(float wx, float wy, float wz);

// Project world AABB (halfW on X, halfD on Y, halfH on Z) via all 8 corners.
bool WorldBoxToScreenRect(
	float cx, float cy, float cz,
	float halfW, float halfH, float halfD,
	float& outMinX, float& outMinY, float& outMaxX, float& outMaxY);

} // namespace sfc
