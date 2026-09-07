#include "render/WorldToScreen.hpp"
#include "core/Log.hpp"
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace sfc {
namespace {

constexpr std::uintptr_t kPreferredBase = 0x00400000;
constexpr std::uintptr_t kThePlayerAbs = 0x011DEA3C;
constexpr std::uintptr_t kInterfaceMgrAbs = 0x011D8A80;
constexpr std::uintptr_t kCamera1stAbs = 0x011E07D0;
constexpr std::uintptr_t kCamera3rdAbs = 0x011E07D4;

constexpr int kOff_WorldToCam = 0x94; // NiAVObject size
constexpr int kOff_Port = 0xF8;
constexpr std::ptrdiff_t kOff_WorldRotate = 0x064;
constexpr std::ptrdiff_t kOff_WorldTranslate = 0x088;

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

struct CamFrame {
	bool valid = false;
	int source = 0; // 1=niCam matrix, 2=camNode view, 3=d3d
	float m[16]{};  // clip matrix for ProjectClip (row-vector * M) OR use gamebryo path
	bool gamebryo = false;
	float w2c[16]{};
	float portL = 0, portT = 0, portR = 1, portB = 1;
	float sw = 1280, sh = 720;
	void* niCam = nullptr;
};

CamFrame g_cam{};

void Mul44(const float* a, const float* b, float* o)
{
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			o[i * 4 + j] =
				a[i * 4 + 0] * b[0 * 4 + j] +
				a[i * 4 + 1] * b[1 * 4 + j] +
				a[i * 4 + 2] * b[2 * 4 + j] +
				a[i * 4 + 3] * b[3 * 4 + j];
}

bool MatrixOk(const float* m)
{
	float s = 0.f;
	for (int i = 0; i < 16; ++i) {
		if (!std::isfinite(m[i])) return false;
		s += std::fabs(m[i]);
	}
	return s > 0.2f && s < 1.0e8f;
}

void* FindNiCamera()
{
	__try {
		void** imSlot = reinterpret_cast<void**>(Rel(kInterfaceMgrAbs));
		void* im = imSlot ? *imSlot : nullptr;
		if (!ValidUserPtr(im)) return nullptr;

		void* graphs[2] = {
			*reinterpret_cast<void**>(reinterpret_cast<char*>(im) + 0x04),
			*reinterpret_cast<void**>(reinterpret_cast<char*>(im) + 0x08)
		};

		// NiNode is 0xAC → SceneGraph::camera is typically at +0xAC (not 0xDC from stale OBSE layout).
		static const int kCamOffs[] = { 0xAC, 0xB0, 0xB4, 0xC0, 0xC4, 0xDC };
		for (void* sg : graphs) {
			if (!ValidUserPtr(sg)) continue;
			for (int off : kCamOffs) {
				void* cam = *reinterpret_cast<void**>(reinterpret_cast<char*>(sg) + off);
				if (!ValidUserPtr(cam)) continue;
				const float* w2c = reinterpret_cast<const float*>(reinterpret_cast<char*>(cam) + kOff_WorldToCam);
				if (MatrixOk(w2c)) return cam;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
	return nullptr;
}

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

bool BuildViewProjFromNode(void* node, float sw, float sh, float* outM)
{
	__try {
		const float* rot = reinterpret_cast<const float*>(reinterpret_cast<char*>(node) + kOff_WorldRotate);
		const float* tr = reinterpret_cast<const float*>(reinterpret_cast<char*>(node) + kOff_WorldTranslate);
		if (!MatrixOk(rot)) return false; // first 9 floats of rot are matrix; hacky but ok-ish

		// Rows as camera axes (Gamebryo world rotation)
		float R[3] = { rot[0], rot[1], rot[2] };
		float U[3] = { rot[3], rot[4], rot[5] };
		float F[3] = { rot[6], rot[7], rot[8] };
		const float eye[3] = { tr[0], tr[1], tr[2] };

		auto len2 = [](const float* v) { return v[0] * v[0] + v[1] * v[1] + v[2] * v[2]; };
		if (len2(R) < 0.01f || len2(F) < 0.01f) return false;

		// View: x=dot(R,p-eye), y=dot(U,p-eye), z=dot(F,p-eye)
		float view[16]{};
		view[0] = R[0]; view[4] = R[1]; view[8] = R[2];
		view[12] = -(R[0] * eye[0] + R[1] * eye[1] + R[2] * eye[2]);
		view[1] = U[0]; view[5] = U[1]; view[9] = U[2];
		view[13] = -(U[0] * eye[0] + U[1] * eye[1] + U[2] * eye[2]);
		view[2] = F[0]; view[6] = F[1]; view[10] = F[2];
		view[14] = -(F[0] * eye[0] + F[1] * eye[1] + F[2] * eye[2]);
		view[15] = 1.f;

		const float fovY = 75.f * 3.14159265f / 180.f;
		const float aspect = sw / (sh > 1.f ? sh : 1.f);
		const float fy = 1.f / std::tanf(fovY * 0.5f);
		const float zn = 5.f, zf = 300000.f;
		float proj[16]{};
		proj[0] = fy / aspect;
		proj[5] = fy;
		proj[10] = zf / (zf - zn);
		proj[11] = 1.f;
		proj[14] = (-zn * zf) / (zf - zn);

		Mul44(view, proj, outM);
		return MatrixOk(outM);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool ProjectGamebryo(const CamFrame& cam, float wx, float wy, float wz, float& sx, float& sy)
{
	const float* w2c = cam.w2c;
	float fX = w2c[0] * wx + w2c[1] * wy + w2c[2] * wz + w2c[3];
	float fY = w2c[4] * wx + w2c[5] * wy + w2c[6] * wz + w2c[7];
	float fW = w2c[12] * wx + w2c[13] * wy + w2c[14] * wz + w2c[15];
	if (fW <= 1.0e-5f) return false;
	fX /= fW; fY /= fW;
	fX = fX * 0.5f + 0.5f;
	fY = fY * 0.5f + 0.5f;
	fX = cam.portL + fX * (cam.portR - cam.portL);
	fY = cam.portT + fY * (cam.portB - cam.portT);
	sx = fX * cam.sw;
	sy = fY * cam.sh;
	return std::isfinite(sx) && std::isfinite(sy);
}

bool ProjectClip(const CamFrame& cam, float wx, float wy, float wz, float& sx, float& sy)
{
	const float* m = cam.m;
	const float x = wx * m[0] + wy * m[4] + wz * m[8] + m[12];
	const float y = wx * m[1] + wy * m[5] + wz * m[9] + m[13];
	const float w = wx * m[3] + wy * m[7] + wz * m[11] + m[15];
	if (w <= 0.001f) return false;
	const float ndcX = x / w;
	const float ndcY = y / w;
	sx = (ndcX + 1.f) * 0.5f * cam.sw;
	sy = (1.f - ndcY) * 0.5f * cam.sh;
	return std::isfinite(sx) && std::isfinite(sy);
}

bool CaptureD3D(IDirect3DDevice9* device, CamFrame& out)
{
	if (!device) return false;
	D3DVIEWPORT9 vp{};
	if (FAILED(device->GetViewport(&vp)) || vp.Width < 16) return false;
	D3DMATRIX view{}, proj{};
	if (FAILED(device->GetTransform(D3DTS_VIEW, &view))) return false;
	if (FAILED(device->GetTransform(D3DTS_PROJECTION, &proj))) return false;
	float v[16], p[16];
	std::memcpy(v, &view, sizeof(v));
	std::memcpy(p, &proj, sizeof(p));
	const float rot = std::fabs(v[1]) + std::fabs(v[2]) + std::fabs(v[4]) + std::fabs(v[8]);
	if (rot < 0.05f) return false;
	Mul44(v, p, out.m);
	out.sw = static_cast<float>(vp.Width);
	out.sh = static_cast<float>(vp.Height);
	out.gamebryo = false;
	out.source = 3;
	out.valid = true;
	return true;
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

	// 1) NiCamera world-to-cam from SceneGraph (probe offsets — 0xAC is correct for FNV NiNode size)
	if (void* cam = FindNiCamera()) {
		g_cam.niCam = cam;
		__try {
			const float* src = reinterpret_cast<const float*>(reinterpret_cast<char*>(cam) + kOff_WorldToCam);
			if (MatrixOk(src)) {
				std::memcpy(g_cam.w2c, src, sizeof(g_cam.w2c));
				g_cam.gamebryo = true;
				g_cam.portL = 0; g_cam.portT = 0; g_cam.portR = 1; g_cam.portB = 1;
				const float* port = reinterpret_cast<const float*>(reinterpret_cast<char*>(cam) + kOff_Port);
				if (std::isfinite(port[0]) && (port[2] - port[0]) > 0.01f && (port[2] - port[0]) <= 1.05f) {
					g_cam.portL = port[0]; g_cam.portT = port[1];
					g_cam.portR = port[2]; g_cam.portB = port[3];
				}
				g_cam.source = 1;
				g_cam.valid = true;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}

	// 2) 1st/3rd person camera NiNode (tracks look direction)
	if (!g_cam.valid) {
		if (void* node = FindCameraNode()) {
			if (BuildViewProjFromNode(node, sw, sh, g_cam.m)) {
				g_cam.gamebryo = false;
				g_cam.source = 2;
				g_cam.valid = true;
			}
		}
	}

	// 3) D3D fixed-function (before ImGui) — last resort
	if (!g_cam.valid) {
		CaptureD3D(device, g_cam);
	}

	static int cool = 0;
	if ((cool++ % 600) == 0) {
		SFC_DBG("W2S source=%d valid=%d cam=%p sw=%.0f sh=%.0f",
			g_cam.source, g_cam.valid ? 1 : 0, g_cam.niCam, g_cam.sw, g_cam.sh);
	}
}

ScreenPos WorldToScreen(float wx, float wy, float wz)
{
	ScreenPos out{};
	if (!g_cam.valid) return out;
	__try {
		bool ok = false;
		if (g_cam.gamebryo) ok = ProjectGamebryo(g_cam, wx, wy, wz, out.x, out.y);
		else ok = ProjectClip(g_cam, wx, wy, wz, out.x, out.y);
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

	const float corners[8][3] = {
		{cx - halfW, cy - halfD, cz},
		{cx + halfW, cy - halfD, cz},
		{cx - halfW, cy + halfD, cz},
		{cx + halfW, cy + halfD, cz},
		{cx - halfW, cy - halfD, cz + halfH * 2.f},
		{cx + halfW, cy - halfD, cz + halfH * 2.f},
		{cx - halfW, cy + halfD, cz + halfH * 2.f},
		{cx + halfW, cy + halfD, cz + halfH * 2.f},
	};

	outMinX = 1e12f; outMinY = 1e12f;
	outMaxX = -1e12f; outMaxY = -1e12f;
	int okCount = 0;
	for (const auto& c : corners) {
		ScreenPos sp = WorldToScreen(c[0], c[1], c[2]);
		if (!sp.ok) continue;
		++okCount;
		outMinX = (std::min)(outMinX, sp.x);
		outMinY = (std::min)(outMinY, sp.y);
		outMaxX = (std::max)(outMaxX, sp.x);
		outMaxY = (std::max)(outMaxY, sp.y);
	}
	if (okCount < 2) return false;
	if (outMaxX - outMinX < 4.f) outMaxX = outMinX + 4.f;
	if (outMaxY - outMinY < 4.f) outMaxY = outMinY + 4.f;
	if (outMaxX - outMinX > g_cam.sw * 2.f) return false;
	if (outMaxY - outMinY > g_cam.sh * 2.f) return false;
	return true;
}

} // namespace sfc
