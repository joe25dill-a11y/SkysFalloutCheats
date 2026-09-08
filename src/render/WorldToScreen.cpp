#include "render/WorldToScreen.hpp"
#include "core/Log.hpp"
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace sfc {
namespace {

constexpr std::uintptr_t kPreferredBase = 0x00400000;
constexpr std::uintptr_t kThePlayerAbs = 0x011DEA3C;
constexpr std::uintptr_t kCamera1stAbs = 0x011E07D0;
constexpr std::uintptr_t kCamera3rdAbs = 0x011E07D4;

constexpr std::ptrdiff_t kOff_RotX = 0x024;
constexpr std::ptrdiff_t kOff_PosX = 0x030;
constexpr std::ptrdiff_t kOff_WorldTranslate = 0x08C;

constexpr float kPi = 3.14159265f;
constexpr float kDeg = 180.f / kPi;

std::uintptr_t Rel(std::uintptr_t absAddr)
{
	const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
	return base + (absAddr - kPreferredBase);
}

bool ValidUserPtr(const void* p)
{
	const auto v = reinterpret_cast<std::uintptr_t>(p);
	return v > 0x10000 && v < 0xFFF00000;
}

float NormAngle(float a)
{
	while (a > kPi) a -= 2.f * kPi;
	while (a < -kPi) a += 2.f * kPi;
	return a;
}

float Dot3(const float* a, const float* b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Cross3(const float* a, const float* b, float* o)
{
	o[0] = a[1] * b[2] - a[2] * b[1];
	o[1] = a[2] * b[0] - a[0] * b[2];
	o[2] = a[0] * b[1] - a[1] * b[0];
}

void Norm3(float* v)
{
	const float L = std::sqrt(Dot3(v, v));
	if (L > 1.0e-4f) {
		v[0] /= L; v[1] /= L; v[2] /= L;
	}
}

struct CamFrame {
	bool valid = false;
	int source = 0;
	float sw = 1280, sh = 720;
	float eyeX = 0, eyeY = 0, eyeZ = 0;
	float yaw = 0;
	float pitch = 0;
	float fovY = 82.f * kPi / 180.f;
	bool hasLook = false;
	float forward[3]{};
	float right[3]{};
	float up[3]{};
};

CamFrame g_cam{};

void* FindCameraNode()
{
	__try {
		void** n3 = reinterpret_cast<void**>(Rel(kCamera3rdAbs));
		void** n1 = reinterpret_cast<void**>(Rel(kCamera1stAbs));
		if (n3 && ValidUserPtr(*n3)) return *n3;
		if (n1 && ValidUserPtr(*n1)) return *n1;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
	return nullptr;
}

bool ReadPlayerPose(float* px, float* py, float* pz, float* rx, float* ry, float* rz)
{
	__try {
		void** slot = reinterpret_cast<void**>(Rel(kThePlayerAbs));
		void* player = slot ? *slot : nullptr;
		if (!ValidUserPtr(player)) return false;
		*rx = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_RotX);
		*ry = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_RotX + 4);
		*rz = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_RotX + 8);
		*px = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX);
		*py = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX + 4);
		*pz = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX + 8);
		return std::isfinite(*px) && std::isfinite(*py) && std::isfinite(*pz)
			&& std::isfinite(*rx) && std::isfinite(*rz);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool ReadNodeTranslate(void* node, float* x, float* y, float* z)
{
	__try {
		if (!ValidUserPtr(node)) return false;
		const float* t = reinterpret_cast<const float*>(reinterpret_cast<char*>(node) + kOff_WorldTranslate);
		if (!std::isfinite(t[0]) || !std::isfinite(t[1]) || !std::isfinite(t[2])) return false;
		*x = t[0]; *y = t[1]; *z = t[2];
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// Player REFR rot: yaw=rotZ, pitch=rotX (same as v17e — that path drew boxes).
void BasisFromPlayerRot(float yaw, float pitch, float* forward, float* right, float* up)
{
	const float cy = std::cosf(yaw);
	const float sy = std::sinf(yaw);
	const float cp = std::cosf(pitch);
	const float sp = std::sinf(pitch);

	forward[0] = sy * cp;
	forward[1] = cy * cp;
	forward[2] = -sp; // FNV rotX: looking up → negative; screen-up needs +Z look

	right[0] = cy;
	right[1] = -sy;
	right[2] = 0.f;

	Cross3(right, forward, up);
	Norm3(forward);
	Norm3(right);
	Norm3(up);
}

bool ProjectLook(const CamFrame& cam, float wx, float wy, float wz, float& sx, float& sy)
{
	if (!cam.hasLook) return false;
	const float d[3] = { wx - cam.eyeX, wy - cam.eyeY, wz - cam.eyeZ };
	const float z = Dot3(d, cam.forward);
	if (z < 8.f) return false;

	const float x = Dot3(d, cam.right);
	const float y = Dot3(d, cam.up);
	const float aspect = cam.sw / (cam.sh > 1.f ? cam.sh : 1.f);
	const float tanHalf = std::tanf(cam.fovY * 0.5f);
	const float nx = (x / z) / (tanHalf * aspect);
	const float ny = (y / z) / tanHalf;

	sx = (nx + 1.f) * 0.5f * cam.sw;
	sy = (1.f - (ny + 1.f) * 0.5f) * cam.sh;
	return std::isfinite(sx) && std::isfinite(sy);
}

bool ProjectAngular(const CamFrame& cam, float wx, float wy, float wz, float& sx, float& sy)
{
	const float dx = wx - cam.eyeX;
	const float dy = wy - cam.eyeY;
	const float dz = wz - cam.eyeZ;
	const float distXY = std::sqrt(dx * dx + dy * dy);
	if (distXY < 4.f && std::fabs(dz) < 4.f) return false;

	const float yawTo = std::atan2(dx, dy);
	const float pitchTo = std::atan2(dz, (std::max)(distXY, 1.f));
	const float dyaw = NormAngle(yawTo - cam.yaw);
	const float dpitch = NormAngle(pitchTo - cam.pitch);

	if (std::fabs(dyaw) > (120.f / kDeg)) return false;

	const float aspect = cam.sw / (cam.sh > 1.f ? cam.sh : 1.f);
	const float halfFovY = cam.fovY * 0.5f;
	const float halfFovX = halfFovY * aspect;

	sx = cam.sw * 0.5f + (dyaw / halfFovX) * (cam.sw * 0.5f);
	sy = cam.sh * 0.5f - (dpitch / halfFovY) * (cam.sh * 0.5f);
	return std::isfinite(sx) && std::isfinite(sy);
}

} // namespace

void CaptureCameraForFrame(IDirect3DDevice9* device)
{
	g_cam = {};
	float sw = 1280.f, sh = 720.f;
	if (device) {
		D3DVIEWPORT9 vp{};
		if (SUCCEEDED(device->GetViewport(&vp)) && vp.Width > 0) {
			sw = static_cast<float>(vp.Width);
			sh = static_cast<float>(vp.Height);
		}
	}
	g_cam.sw = sw;
	g_cam.sh = sh;
	g_cam.fovY = 82.f * kPi / 180.f;

	float px = 0, py = 0, pz = 0, rx = 0, ry = 0, rz = 0;
	if (!ReadPlayerPose(&px, &py, &pz, &rx, &ry, &rz)) return;

	g_cam.yaw = rz;
	g_cam.pitch = -rx; // view pitch: looking up = positive

	// Eye: camera node translate only (position). Do NOT use node rotation — it was -80° garbage.
	g_cam.eyeX = px;
	g_cam.eyeY = py;
	g_cam.eyeZ = pz + 120.f;
	if (void* node = FindCameraNode()) {
		float cx, cy, cz;
		if (ReadNodeTranslate(node, &cx, &cy, &cz)) {
			g_cam.eyeX = cx;
			g_cam.eyeY = cy;
			g_cam.eyeZ = cz;
			g_cam.source = 6;
		} else {
			g_cam.source = 5;
		}
	} else {
		g_cam.source = 5;
	}

	BasisFromPlayerRot(rz, rx, g_cam.forward, g_cam.right, g_cam.up);
	g_cam.hasLook = true;
	g_cam.valid = true;

	static int cool = 0;
	if ((cool++ % 300) == 0) {
		float tsx = -1, tsy = -1;
		const bool ok = ProjectLook(g_cam, px, py, pz + 40.f, tsx, tsy)
			|| ProjectAngular(g_cam, px, py, pz + 40.f, tsx, tsy);
		SFC_LOG("W2S src=%d ok=%d xy=%.0f,%.0f viewPitch=%.1f",
			g_cam.source, ok ? 1 : 0, tsx, tsy, g_cam.pitch * kDeg);
	}
}

ScreenPos WorldToScreen(float wx, float wy, float wz)
{
	ScreenPos out{};
	if (!g_cam.valid) return out;
	__try {
		bool ok = ProjectLook(g_cam, wx, wy, wz, out.x, out.y);
		if (!ok) ok = ProjectAngular(g_cam, wx, wy, wz, out.x, out.y);
		out.ok = ok;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		out.ok = false;
	}
	return out;
}

bool WorldBoxToScreenRect(
	float cx, float cy, float cz,
	float halfW, float halfH, float halfD,
	float& outMinX, float& outMinY, float& outMaxX, float& outMaxY)
{
	if (!g_cam.valid) return false;

	const float midZ = cz + halfH;
	ScreenPos mid = WorldToScreen(cx, cy, midZ);
	if (!mid.ok) {
		mid = WorldToScreen(cx, cy, cz);
		if (!mid.ok) return false;
	}

	ScreenPos head = WorldToScreen(cx, cy, cz + halfH * 2.f);
	ScreenPos feet = WorldToScreen(cx, cy, cz);
	float h = 40.f;
	if (head.ok && feet.ok)
		h = std::fabs(head.y - feet.y);
	if (h < 16.f) h = 16.f;
	if (h > 200.f) h = 200.f;
	float w = h * (halfH > 1.f ? (halfW / halfH) : 0.4f);
	if (w < 12.f) w = 12.f;
	if (w > 140.f) w = 140.f;

	outMinX = mid.x - w * 0.5f;
	outMaxX = mid.x + w * 0.5f;
	outMinY = mid.y - h * 0.5f;
	outMaxY = mid.y + h * 0.5f;
	(void)halfD;
	return true;
}

} // namespace sfc
