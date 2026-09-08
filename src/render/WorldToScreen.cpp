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
constexpr std::uintptr_t kInterfaceMgrAbs = 0x011D8A80;
constexpr std::uintptr_t kWorldSceneGraphAbs = 0x011F91C8;

constexpr std::ptrdiff_t kOff_RotX = 0x024;
constexpr std::ptrdiff_t kOff_PosX = 0x030;
// NVSE NiAVObject: world RotAndTranslate at 0x064 → translate at 0x088.
constexpr std::ptrdiff_t kOff_WorldRotate = 0x064;
constexpr std::ptrdiff_t kOff_WorldTranslate = 0x088;
constexpr std::ptrdiff_t kOff_WorldTranslateAlt = 0x08C; // JIP-style; try if 0x088 fails
constexpr std::ptrdiff_t kOff_SceneGraphCamera = 0x0DC;
constexpr std::ptrdiff_t kOff_SceneGraphFov = 0x0EC;
constexpr std::ptrdiff_t kOff_NiFrustum = 0x0EC; // NiCamera

constexpr float kPi = 3.14159265f;
constexpr float kDeg = 180.f / kPi;
constexpr float kDefaultFovY = 75.f * kPi / 180.f;
constexpr float kNearZ = 1.f;

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

bool FovLooksSane(float fovRad)
{
	const float deg = fovRad * kDeg;
	return std::isfinite(deg) && deg >= 20.f && deg <= 150.f;
}

struct CamFrame {
	bool valid = false;
	int source = 0;
	int fovSource = 0;
	float sw = 1280, sh = 720;
	float eyeX = 0, eyeY = 0, eyeZ = 0;
	float yaw = 0;
	float pitch = 0;
	float fovY = kDefaultFovY;
	bool hasLook = false;
	float forward[3]{};
	float right[3]{};
	float up[3]{};
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

bool ReadNodeTranslateAt(void* node, std::ptrdiff_t off, float* x, float* y, float* z)
{
	__try {
		if (!ValidUserPtr(node)) return false;
		const float* t = reinterpret_cast<const float*>(reinterpret_cast<char*>(node) + off);
		if (!std::isfinite(t[0]) || !std::isfinite(t[1]) || !std::isfinite(t[2])) return false;
		// Reject absurd world coords.
		if (std::fabs(t[0]) > 500000.f || std::fabs(t[1]) > 500000.f || std::fabs(t[2]) > 500000.f)
			return false;
		*x = t[0]; *y = t[1]; *z = t[2];
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool ReadNodeTranslate(void* node, float* x, float* y, float* z)
{
	if (ReadNodeTranslateAt(node, kOff_WorldTranslate, x, y, z)) return true;
	return ReadNodeTranslateAt(node, kOff_WorldTranslateAlt, x, y, z);
}

bool ReadNodeRotate(void* node, float rot[9])
{
	__try {
		if (!ValidUserPtr(node) || !rot) return false;
		const float* r = reinterpret_cast<const float*>(reinterpret_cast<char*>(node) + kOff_WorldRotate);
		for (int i = 0; i < 9; ++i) {
			if (!std::isfinite(r[i])) return false;
			rot[i] = r[i];
		}
		// Identity-ish garbage check: row lengths should be ~1.
		auto rowLen = [&](int row) {
			const float* v = rot + row * 3;
			return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
		};
		if (rowLen(0) < 0.5f || rowLen(1) < 0.5f || rowLen(2) < 0.5f) return false;
		if (rowLen(0) > 1.5f || rowLen(1) > 1.5f || rowLen(2) > 1.5f) return false;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void* FindPersonCameraNode()
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

void* FindWorldSceneGraph()
{
	__try {
		void** slot = reinterpret_cast<void**>(Rel(kWorldSceneGraphAbs));
		if (slot && ValidUserPtr(*slot)) return *slot;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
	return nullptr;
}

void* FindWorldNiCamera()
{
	__try {
		void* sg = FindWorldSceneGraph();
		if (!sg) return nullptr;
		void* cam = *reinterpret_cast<void**>(reinterpret_cast<char*>(sg) + kOff_SceneGraphCamera);
		if (!ValidUserPtr(cam)) return nullptr;
		return cam;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

bool ReadSceneGraphFov(void* sg, float* outFovRad)
{
	__try {
		if (!ValidUserPtr(sg) || !outFovRad) return false;
		const float deg = *reinterpret_cast<float*>(reinterpret_cast<char*>(sg) + kOff_SceneGraphFov);
		const float rad = deg * (kPi / 180.f);
		if (!FovLooksSane(rad)) return false;
		*outFovRad = rad;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool ReadFrustumFov(void* niCam, float* outFovRad)
{
	__try {
		if (!ValidUserPtr(niCam) || !outFovRad) return false;
		const float* fr = reinterpret_cast<const float*>(reinterpret_cast<char*>(niCam) + kOff_NiFrustum);
		// l,r,t,b,n,f
		const float t = fr[2], n = fr[4];
		if (!std::isfinite(t) || !std::isfinite(n) || n <= 1.0e-4f) return false;
		const float fov = 2.f * std::atanf(std::fabs(t) / n);
		if (!FovLooksSane(fov)) return false;
		*outFovRad = fov;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool TryFovFromD3D(IDirect3DDevice9* device, float* outFovRad)
{
	if (!device || !outFovRad) return false;
	D3DMATRIX proj{};
	if (FAILED(device->GetTransform(D3DTS_PROJECTION, &proj))) return false;
	if (!std::isfinite(proj._22) || std::fabs(proj._22) < 1.0e-4f) return false;
	const float fov = 2.f * std::atanf(1.f / std::fabs(proj._22));
	if (!FovLooksSane(fov)) return false;
	if (std::fabs(proj._33) < 1.0e-5f && std::fabs(proj._43) < 1.0e-5f) return false;
	*outFovRad = fov;
	return true;
}

bool TryFovFromInterfaceSceneGraphs(float* outFovRad)
{
	__try {
		void** imSlot = reinterpret_cast<void**>(Rel(kInterfaceMgrAbs));
		void* im = imSlot ? *imSlot : nullptr;
		if (!ValidUserPtr(im)) return false;
		void* sg0 = *reinterpret_cast<void**>(reinterpret_cast<char*>(im) + 0x04);
		void* sg1 = *reinterpret_cast<void**>(reinterpret_cast<char*>(im) + 0x08);
		float f0 = 0, f1 = 0;
		const bool ok0 = ReadSceneGraphFov(sg0, &f0);
		const bool ok1 = ReadSceneGraphFov(sg1, &f1);
		if (ok0 && ok1) {
			*outFovRad = (f0 > f1) ? f0 : f1;
			return true;
		}
		if (ok0) { *outFovRad = f0; return true; }
		if (ok1) { *outFovRad = f1; return true; }
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
	return false;
}

void BasisFromPlayerRot(float yaw, float pitch, float* forward, float* right, float* up)
{
	const float cy = std::cosf(yaw);
	const float sy = std::sinf(yaw);
	const float cp = std::cosf(pitch);
	const float sp = std::sinf(pitch);

	forward[0] = sy * cp;
	forward[1] = cy * cp;
	forward[2] = -sp;

	right[0] = cy;
	right[1] = -sy;
	right[2] = 0.f;

	Cross3(right, forward, up);
	Norm3(forward);
	Norm3(right);
	Norm3(up);
}

// Build orthonormal basis from NiMatrix33 using a look convention.
// conv: 0 = -col2 (NiCamera -Z), 1 = +col1 (Bethesda Y-forward), 2 = -row2, 3 = +col2
bool BasisFromMatrix(const float rot[9], int conv, float* forward, float* right, float* up)
{
	float f[3], r[3], u[3];
	switch (conv) {
	case 0: // look = -Z column
		r[0] = rot[0]; r[1] = rot[3]; r[2] = rot[6];
		u[0] = rot[1]; u[1] = rot[4]; u[2] = rot[7];
		f[0] = -rot[2]; f[1] = -rot[5]; f[2] = -rot[8];
		break;
	case 1: // look = +Y column (common actor/camera bone)
		r[0] = rot[0]; r[1] = rot[3]; r[2] = rot[6];
		f[0] = rot[1]; f[1] = rot[4]; f[2] = rot[7];
		u[0] = rot[2]; u[1] = rot[5]; u[2] = rot[8];
		break;
	case 2: // look = -row2
		r[0] = rot[0]; r[1] = rot[1]; r[2] = rot[2];
		u[0] = rot[3]; u[1] = rot[4]; u[2] = rot[5];
		f[0] = -rot[6]; f[1] = -rot[7]; f[2] = -rot[8];
		break;
	case 3: // look = +Z column
		r[0] = rot[0]; r[1] = rot[3]; r[2] = rot[6];
		u[0] = rot[1]; u[1] = rot[4]; u[2] = rot[7];
		f[0] = rot[2]; f[1] = rot[5]; f[2] = rot[8];
		break;
	default:
		return false;
	}

	Norm3(f);
	Norm3(r);
	// Re-orthogonalize up from right × forward to avoid shear.
	Cross3(r, f, u);
	Norm3(u);
	Cross3(f, u, r);
	Norm3(r);

	if (Dot3(f, f) < 0.5f || Dot3(r, r) < 0.5f || Dot3(u, u) < 0.5f) return false;

	forward[0] = f[0]; forward[1] = f[1]; forward[2] = f[2];
	right[0] = r[0]; right[1] = r[1]; right[2] = r[2];
	up[0] = u[0]; up[1] = u[1]; up[2] = u[2];
	return true;
}

bool ProjectLook(const CamFrame& cam, float wx, float wy, float wz, float& sx, float& sy)
{
	if (!cam.hasLook) return false;
	const float d[3] = { wx - cam.eyeX, wy - cam.eyeY, wz - cam.eyeZ };
	const float z = Dot3(d, cam.forward);
	if (z < kNearZ) return false;

	const float x = Dot3(d, cam.right);
	const float y = Dot3(d, cam.up);
	const float aspect = cam.sw / (cam.sh > 1.f ? cam.sh : 1.f);
	const float tanHalf = std::tanf(cam.fovY * 0.5f);
	if (tanHalf < 1.0e-4f) return false;
	const float nx = (x / z) / (tanHalf * aspect);
	const float ny = (y / z) / tanHalf;

	sx = (nx + 1.f) * 0.5f * cam.sw;
	sy = (1.f - (ny + 1.f) * 0.5f) * cam.sh;
	return std::isfinite(sx) && std::isfinite(sy);
}

// Reject camera frames that would recreate the video bug (turn → everything "behind").
bool CamFrameSanity(const CamFrame& cam, const float* playerFwd)
{
	if (!cam.hasLook || !cam.valid) return false;

	// Looking almost straight up/down is only OK if player is too.
	const float pitch = std::asinf((std::max)(-1.f, (std::min)(1.f, cam.forward[2])));
	if (std::fabs(pitch) > (75.f / kDeg) && playerFwd && std::fabs(playerFwd[2]) < 0.55f)
		return false;

	if (playerFwd) {
		const float align = Dot3(cam.forward, playerFwd);
		// Must share hemisphere with body facing in normal play.
		if (align < 0.15f) return false;
	}

	float sx = 0, sy = 0;
	const float wx = cam.eyeX + cam.forward[0] * 256.f;
	const float wy = cam.eyeY + cam.forward[1] * 256.f;
	const float wz = cam.eyeZ + cam.forward[2] * 256.f;
	if (!ProjectLook(cam, wx, wy, wz, sx, sy)) return false;

	const float cx = cam.sw * 0.5f;
	const float cy = cam.sh * 0.5f;
	const float ndx = (sx - cx) / (cam.sw > 1.f ? cam.sw : 1.f);
	const float ndy = (sy - cy) / (cam.sh > 1.f ? cam.sh : 1.f);
	return std::fabs(ndx) < 0.22f && std::fabs(ndy) < 0.22f;
}

bool TryFillFromNode(void* node, CamFrame& cam, const float* playerFwd, int baseSource)
{
	float eyeX, eyeY, eyeZ;
	float rot[9];
	if (!ReadNodeTranslate(node, &eyeX, &eyeY, &eyeZ)) return false;
	if (!ReadNodeRotate(node, rot)) return false;

	cam.eyeX = eyeX;
	cam.eyeY = eyeY;
	cam.eyeZ = eyeZ;

	for (int conv = 0; conv < 4; ++conv) {
		if (!BasisFromMatrix(rot, conv, cam.forward, cam.right, cam.up)) continue;
		cam.hasLook = true;
		cam.valid = true;
		cam.source = baseSource + conv;
		if (CamFrameSanity(cam, playerFwd)) return true;
	}
	cam.hasLook = false;
	cam.valid = false;
	return false;
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

	g_cam.fovY = kDefaultFovY;
	g_cam.fovSource = 0;
	float liveFov = 0.f;
	if (TryFovFromD3D(device, &liveFov)) {
		g_cam.fovY = liveFov;
		g_cam.fovSource = 1;
	} else if (void* sg = FindWorldSceneGraph(); ReadSceneGraphFov(sg, &liveFov)) {
		g_cam.fovY = liveFov;
		g_cam.fovSource = 2;
	} else if (TryFovFromInterfaceSceneGraphs(&liveFov)) {
		g_cam.fovY = liveFov;
		g_cam.fovSource = 3;
	}

	float px = 0, py = 0, pz = 0, rx = 0, ry = 0, rz = 0;
	if (!ReadPlayerPose(&px, &py, &pz, &rx, &ry, &rz)) return;

	g_cam.yaw = rz;
	g_cam.pitch = -rx;

	float playerFwd[3], playerRight[3], playerUp[3];
	BasisFromPlayerRot(rz, rx, playerFwd, playerRight, playerUp);

	bool got = false;

	// 1) Real world NiCamera (render camera) — preferred when sane.
	if (void* worldCam = FindWorldNiCamera()) {
		float frFov = 0.f;
		if (ReadFrustumFov(worldCam, &frFov)) {
			g_cam.fovY = frFov;
			g_cam.fovSource = 4;
		}
		got = TryFillFromNode(worldCam, g_cam, playerFwd, 20);
	}

	// 2) 1st/3rd person camera node with matrix look (not player body rot).
	if (!got) {
		if (void* node = FindPersonCameraNode())
			got = TryFillFromNode(node, g_cam, playerFwd, 10);
	}

	// 3) Safe fallback that previously drew boxes: cam eye + player look.
	if (!got) {
		g_cam.eyeX = px;
		g_cam.eyeY = py;
		g_cam.eyeZ = pz + 120.f;
		if (void* node = FindPersonCameraNode()) {
			float cx, cy, cz;
			if (ReadNodeTranslate(node, &cx, &cy, &cz)) {
				g_cam.eyeX = cx;
				g_cam.eyeY = cy;
				g_cam.eyeZ = cz;
			}
		}
		g_cam.forward[0] = playerFwd[0];
		g_cam.forward[1] = playerFwd[1];
		g_cam.forward[2] = playerFwd[2];
		g_cam.right[0] = playerRight[0];
		g_cam.right[1] = playerRight[1];
		g_cam.right[2] = playerRight[2];
		g_cam.up[0] = playerUp[0];
		g_cam.up[1] = playerUp[1];
		g_cam.up[2] = playerUp[2];
		g_cam.hasLook = true;
		g_cam.valid = true;
		g_cam.source = 5;
	}

	static int cool = 0;
	if ((cool++ % 120) == 0) {
		float tsx = -1, tsy = -1;
		const float fx = g_cam.eyeX + g_cam.forward[0] * 256.f;
		const float fy = g_cam.eyeY + g_cam.forward[1] * 256.f;
		const float fz = g_cam.eyeZ + g_cam.forward[2] * 256.f;
		const bool ok = ProjectLook(g_cam, fx, fy, fz, tsx, tsy);
		SFC_LOG("W2S src=%d fovSrc=%d fov=%.1f ok=%d xy=%.0f,%.0f pitch=%.1f align=%.2f",
			g_cam.source, g_cam.fovSource, g_cam.fovY * kDeg, ok ? 1 : 0,
			tsx, tsy, std::asinf((std::max)(-1.f, (std::min)(1.f, g_cam.forward[2]))) * kDeg,
			Dot3(g_cam.forward, playerFwd));
	}
}

bool GetEspCamInfo(EspCamInfo& out)
{
	out = {};
	if (!g_cam.valid) return false;
	out.valid = true;
	out.source = g_cam.source;
	out.fovSource = g_cam.fovSource;
	out.sw = g_cam.sw;
	out.sh = g_cam.sh;
	out.eyeX = g_cam.eyeX;
	out.eyeY = g_cam.eyeY;
	out.eyeZ = g_cam.eyeZ;
	out.yawDeg = g_cam.yaw * kDeg;
	out.pitchDeg = g_cam.pitch * kDeg;
	out.fovDeg = g_cam.fovY * kDeg;
	return true;
}

ScreenPos WorldToScreen(float wx, float wy, float wz)
{
	ScreenPos out{};
	if (!g_cam.valid) return out;
	__try {
		out.ok = ProjectLook(g_cam, wx, wy, wz, out.x, out.y);
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

	const float midX = cx;
	const float midY = cy;
	const float midZ = cz + halfH;

	float minX = 1.0e9f, minY = 1.0e9f, maxX = -1.0e9f, maxY = -1.0e9f;
	int okCount = 0;

	const float xs[2] = { midX - halfW, midX + halfW };
	const float ys[2] = { midY - halfD, midY + halfD };
	const float zs[2] = { midZ - halfH, midZ + halfH };

	for (int ix = 0; ix < 2; ++ix) {
		for (int iy = 0; iy < 2; ++iy) {
			for (int iz = 0; iz < 2; ++iz) {
				ScreenPos sp = WorldToScreen(xs[ix], ys[iy], zs[iz]);
				if (!sp.ok) continue;
				minX = (std::min)(minX, sp.x);
				minY = (std::min)(minY, sp.y);
				maxX = (std::max)(maxX, sp.x);
				maxY = (std::max)(maxY, sp.y);
				++okCount;
			}
		}
	}

	if (okCount <= 0) return false;
	outMinX = minX;
	outMinY = minY;
	outMaxX = maxX;
	outMaxY = maxY;
	return true;
}

} // namespace sfc
