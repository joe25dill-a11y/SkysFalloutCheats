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

constexpr std::ptrdiff_t kOff_RotX = 0x024;
constexpr std::ptrdiff_t kOff_PosX = 0x030;

constexpr float kPi = 3.14159265f;
constexpr float kDeg = 180.f / kPi;
constexpr float kNearZ = 4.f;

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
	float fovY = 75.f * kPi / 180.f;
	bool hasLook = false;
	float forward[3]{};
	float right[3]{};
	float up[3]{};
	bool hasD3D = false;
	float vp[16]{}; // column-major view*proj
};

CamFrame g_cam{};

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

// Player REFR rot: yaw=rotZ, pitch=rotX.
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

// Row-major 4x4 multiply (D3D row-vector: v' = v * A * B).
void MulMat4(const float* a, const float* b, float* o)
{
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			o[r * 4 + c] =
				a[r * 4 + 0] * b[0 * 4 + c] +
				a[r * 4 + 1] * b[1 * 4 + c] +
				a[r * 4 + 2] * b[2 * 4 + c] +
				a[r * 4 + 3] * b[3 * 4 + c];
		}
	}
}

bool TryCaptureD3D(IDirect3DDevice9* device, CamFrame& cam)
{
	if (!device) return false;
	D3DMATRIX view{}, proj{};
	if (FAILED(device->GetTransform(D3DTS_VIEW, &view))) return false;
	if (FAILED(device->GetTransform(D3DTS_PROJECTION, &proj))) return false;

	// Gamebryo often leaves these identity / unused at EndScene — bail if so.
	const float tlen = std::sqrt(view._41 * view._41 + view._42 * view._42 + view._43 * view._43);
	const float fdiag = std::fabs(view._11) + std::fabs(view._22) + std::fabs(view._33);
	if (tlen < 8.f && fdiag > 2.9f && fdiag < 3.1f) return false;

	float v[16] = {
		view._11, view._12, view._13, view._14,
		view._21, view._22, view._23, view._24,
		view._31, view._32, view._33, view._34,
		view._41, view._42, view._43, view._44
	};
	float p[16] = {
		proj._11, proj._12, proj._13, proj._14,
		proj._21, proj._22, proj._23, proj._24,
		proj._31, proj._32, proj._33, proj._34,
		proj._41, proj._42, proj._43, proj._44
	};
	MulMat4(v, p, cam.vp);
	cam.hasD3D = true;
	return true;
}

bool ProjectD3D(const CamFrame& cam, float wx, float wy, float wz, float& sx, float& sy, float& wzClip)
{
	const float* m = cam.vp;
	const float x = wx * m[0] + wy * m[4] + wz * m[8] + m[12];
	const float y = wx * m[1] + wy * m[5] + wz * m[9] + m[13];
	const float z = wx * m[2] + wy * m[6] + wz * m[10] + m[14];
	const float w = wx * m[3] + wy * m[7] + wz * m[11] + m[15];
	if (!std::isfinite(w) || std::fabs(w) < 1.0e-4f) return false;
	wzClip = z / w;
	if (wzClip < 0.f) return false;
	const float nx = x / w;
	const float ny = y / w;
	sx = (nx + 1.f) * 0.5f * cam.sw;
	sy = (1.f - (ny + 1.f) * 0.5f) * cam.sh;
	return std::isfinite(sx) && std::isfinite(sy);
}

bool ProjectLook(const CamFrame& cam, float wx, float wy, float wz, float& sx, float& sy, float& depth)
{
	if (!cam.hasLook) return false;
	const float d[3] = { wx - cam.eyeX, wy - cam.eyeY, wz - cam.eyeZ };
	depth = Dot3(d, cam.forward);
	if (depth < kNearZ) return false;

	const float x = Dot3(d, cam.right);
	const float y = Dot3(d, cam.up);
	const float aspect = cam.sw / (cam.sh > 1.f ? cam.sh : 1.f);
	const float tanHalf = std::tanf(cam.fovY * 0.5f);
	const float nx = (x / depth) / (tanHalf * aspect);
	const float ny = (y / depth) / tanHalf;

	sx = (nx + 1.f) * 0.5f * cam.sw;
	sy = (1.f - (ny + 1.f) * 0.5f) * cam.sh;
	return std::isfinite(sx) && std::isfinite(sy);
}

void ClampToScreenEdge(float& sx, float& sy, float sw, float sh)
{
	const float cx = sw * 0.5f;
	const float cy = sh * 0.5f;
	float dx = sx - cx;
	float dy = sy - cy;
	if (std::fabs(dx) < 1.f && std::fabs(dy) < 1.f) {
		sx = sw - 18.f;
		sy = cy;
		return;
	}
	const float margin = 14.f;
	const float maxX = cx - margin;
	const float maxY = cy - margin;
	const float ax = std::fabs(dx) / (maxX > 1.f ? maxX : 1.f);
	const float ay = std::fabs(dy) / (maxY > 1.f ? maxY : 1.f);
	const float s = (std::max)(ax, ay);
	if (s > 1.f) {
		dx /= s;
		dy /= s;
	}
	sx = cx + dx;
	sy = cy + dy;
	sx = (std::max)(margin, (std::min)(sw - margin, sx));
	sy = (std::max)(margin, (std::min)(sh - margin, sy));
}

// Behind / extreme off-angle: place a ping from yaw only.
void EdgeFromYaw(const CamFrame& cam, float wx, float wy, float& sx, float& sy)
{
	const float dx = wx - cam.eyeX;
	const float dy = wy - cam.eyeY;
	const float yawTo = std::atan2(dx, dy);
	const float dyaw = NormAngle(yawTo - cam.yaw);
	// Map ±180° around the border (left/right/behind → edges).
	const float t = dyaw / kPi; // -1 .. 1
	sx = cam.sw * 0.5f + t * (cam.sw * 0.5f - 16.f);
	sy = cam.sh * 0.5f;
	if (std::fabs(dyaw) > (90.f / kDeg))
		sy = cam.sh * 0.78f; // behind → lower edge cue
	ClampToScreenEdge(sx, sy, cam.sw, cam.sh);
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
	g_cam.fovY = 75.f * kPi / 180.f;

	float px = 0, py = 0, pz = 0, rx = 0, ry = 0, rz = 0;
	if (!ReadPlayerPose(&px, &py, &pz, &rx, &ry, &rz)) return;

	g_cam.yaw = rz;
	g_cam.pitch = -rx;

	// Keep eye + look from the SAME source (player). Mixing cam-node eye with
	// player rot made 3rd-person targets fail W2S until you walked into view.
	g_cam.eyeX = px;
	g_cam.eyeY = py;
	g_cam.eyeZ = pz + 128.f;
	g_cam.source = 7;

	BasisFromPlayerRot(rz, rx, g_cam.forward, g_cam.right, g_cam.up);
	g_cam.hasLook = true;

	if (TryCaptureD3D(device, g_cam))
		g_cam.source = 8;

	g_cam.valid = true;

	static int cool = 0;
	if ((cool++ % 300) == 0) {
		float tsx = -1, tsy = -1, depth = 0;
		const float fx = px + g_cam.forward[0] * 200.f;
		const float fy = py + g_cam.forward[1] * 200.f;
		const float fz = pz + 128.f + g_cam.forward[2] * 200.f;
		bool ok = false;
		if (g_cam.hasD3D) {
			float zc = 0;
			ok = ProjectD3D(g_cam, fx, fy, fz, tsx, tsy, zc);
		}
		if (!ok) ok = ProjectLook(g_cam, fx, fy, fz, tsx, tsy, depth);
		SFC_LOG("W2S src=%d d3d=%d ok=%d xy=%.0f,%.0f pitch=%.1f",
			g_cam.source, g_cam.hasD3D ? 1 : 0, ok ? 1 : 0, tsx, tsy, g_cam.pitch * kDeg);
	}
}

ScreenPos WorldToScreen(float wx, float wy, float wz)
{
	ScreenPos out{};
	if (!g_cam.valid) return out;
	__try {
		float depth = 0.f;
		bool ok = false;
		if (g_cam.hasD3D) {
			float zc = 0.f;
			ok = ProjectD3D(g_cam, wx, wy, wz, out.x, out.y, zc);
			if (!ok) out.behind = true;
		}
		if (!ok) {
			ok = ProjectLook(g_cam, wx, wy, wz, out.x, out.y, depth);
			if (!ok) out.behind = (depth < kNearZ);
		}
		out.ok = ok;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		out.ok = false;
	}
	return out;
}

bool ProjectEspPoint(float wx, float wy, float wz, float& sx, float& sy, bool& onScreen)
{
	onScreen = false;
	if (!g_cam.valid) return false;

	ScreenPos sp = WorldToScreen(wx, wy, wz);
	if (sp.ok) {
		sx = sp.x;
		sy = sp.y;
		const float m = 4.f;
		if (sx >= m && sy >= m && sx <= g_cam.sw - m && sy <= g_cam.sh - m) {
			onScreen = true;
			return true;
		}
		ClampToScreenEdge(sx, sy, g_cam.sw, g_cam.sh);
		return true;
	}

	// Behind camera / failed depth — still show a wallhack edge ping.
	EdgeFromYaw(g_cam, wx, wy, sx, sy);
	return true;
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
	if (h > 220.f) h = 220.f;
	float w = h * (halfH > 1.f ? (halfW / halfH) : 0.4f);
	if (w < 12.f) w = 12.f;
	if (w > 160.f) w = 160.f;

	outMinX = mid.x - w * 0.5f;
	outMaxX = mid.x + w * 0.5f;
	outMinY = mid.y - h * 0.5f;
	outMaxY = mid.y + h * 0.5f;
	(void)halfD;
	return true;
}

} // namespace sfc
