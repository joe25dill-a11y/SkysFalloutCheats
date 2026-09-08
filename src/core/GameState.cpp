#include "core/GameState.hpp"
#include "core/Config.hpp"
#include "core/Log.hpp"
#include "core/Diag.hpp"
#include "core/Input.hpp"
#include "core/UiHud.hpp"
#include "render/D3D9Hook.hpp"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <vector>

namespace sfc {
namespace {

constexpr std::uintptr_t kPreferredBase = 0x00400000;
constexpr std::uintptr_t kThePlayerAbs = 0x011DEA3C;
constexpr std::uintptr_t kFormsMapAbs = 0x011C54C0;
constexpr std::uintptr_t kLookupFormAbs = 0x004F9620;
constexpr std::uintptr_t kGetFormByEditorIdAbs = 0x004F9650;
constexpr std::uintptr_t kExtraGetByTypeAbs = 0x00410220;

constexpr std::uint32_t kFormCaps001 = 0x0000000F;
constexpr std::uint32_t kFormGameHour = 0x00000039;
constexpr std::uint32_t kAV_ActionPoints = 12;
constexpr std::uint32_t kAV_Health = 16;
constexpr std::uint32_t kExtraContainerChanges = 0x15;

constexpr std::uint8_t kFormType_TESKey = 0x15;
constexpr std::uint8_t kFormType_TESObjectBOOK = 0x19;
constexpr std::uint8_t kFormType_TESObjectARMO = 0x1A;
constexpr std::uint8_t kFormType_TESObjectCONT = 0x1B;
constexpr std::uint8_t kFormType_TESObjectDOOR = 0x1C;
constexpr std::uint8_t kFormType_TESObjectMISC = 0x1F;
constexpr std::uint8_t kFormType_TESObjectWEAP = 0x28;
constexpr std::uint8_t kFormType_TESAmmo = 0x29;
constexpr std::uint8_t kFormType_TESNPC = 0x2A;
constexpr std::uint8_t kFormType_TESCreature = 0x2B;
constexpr std::uint8_t kFormType_AlchemyItem = 0x2F;
constexpr std::uint8_t kFormType_BGSNote = 0x30;
constexpr std::uint8_t kFormType_Character = 0x3B;
constexpr std::uint8_t kFormType_Creature = 0x3C;

constexpr std::uintptr_t kActorProcessMgrAbs = 0x011E0E80;
constexpr std::ptrdiff_t kOff_HighActors = 0x05C;
constexpr std::ptrdiff_t kOff_MiddleHighActors = 0x000;

constexpr std::ptrdiff_t kOff_PosX = 0x030;
constexpr std::ptrdiff_t kOff_ParentCell = 0x040;
constexpr std::ptrdiff_t kOff_RenderState = 0x064;
constexpr std::ptrdiff_t kOff_RsNiNode = 0x014;       // RenderState::niNode
// JIP NiAVObject: m_transformWorld @ 0x68, translate @ +0x24 → 0x8C
constexpr std::ptrdiff_t kOff_NodeWorldTranslate = 0x08C;
constexpr float kUnitsPerFoot = 128.f / 6.f; // Bethesda: 128 units = 6 feet

// Prefer REFR pos (same space as setpos / player rot). NiNode only as fallback.
bool ReadRefWorldPos(void* refr, float* ox, float* oy, float* oz)
{
	if (!refr || !ox || !oy || !oz) return false;
	__try {
		*ox = *reinterpret_cast<float*>(reinterpret_cast<char*>(refr) + kOff_PosX);
		*oy = *reinterpret_cast<float*>(reinterpret_cast<char*>(refr) + kOff_PosX + 4);
		*oz = *reinterpret_cast<float*>(reinterpret_cast<char*>(refr) + kOff_PosX + 8);
		if (std::isfinite(*ox) && std::isfinite(*oy) && std::isfinite(*oz))
			return true;

		void* rs = *reinterpret_cast<void**>(reinterpret_cast<char*>(refr) + kOff_RenderState);
		if (rs) {
			void* node = *reinterpret_cast<void**>(reinterpret_cast<char*>(rs) + kOff_RsNiNode);
			if (node) {
				const float* t = reinterpret_cast<const float*>(reinterpret_cast<char*>(node) + kOff_NodeWorldTranslate);
				if (std::isfinite(t[0]) && std::isfinite(t[1]) && std::isfinite(t[2])) {
					*ox = t[0]; *oy = t[1]; *oz = t[2];
					return true;
				}
			}
		}
		return false;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}
constexpr std::ptrdiff_t kOff_ExtraList = 0x044;
constexpr std::ptrdiff_t kOff_BaseForm = 0x020;
constexpr std::ptrdiff_t kOff_AVOwner = 0x0A4;
constexpr std::ptrdiff_t kOff_BaseProcess = 0x068;
constexpr std::ptrdiff_t kOff_WeaponInfo = 0x114;
constexpr std::ptrdiff_t kOff_AmmoInfo = 0x118;
constexpr std::ptrdiff_t kOff_CellObjectList = 0x0AC;
constexpr std::ptrdiff_t kOff_CellData = 0x048;       // CellCoordinates*
constexpr std::ptrdiff_t kOff_CellWorldSpace = 0x0C0; // TESWorldSpace*
constexpr std::ptrdiff_t kOff_WsCellMap = 0x030;      // NiTPointerMap<TESObjectCELL>*
constexpr std::ptrdiff_t kOff_ActorBaseFullName = 0x0D0;
constexpr std::ptrdiff_t kOff_WeapFullName = 0x030;
constexpr std::ptrdiff_t kOff_WeapClipRounds = 0x0B0; // BGSClipRoundsForm.clipRounds
constexpr std::ptrdiff_t kOff_ContFullName = 0x030;
constexpr std::ptrdiff_t kOff_FormRefID = 0x00C;
constexpr std::ptrdiff_t kOff_FormTypeID = 0x004;

std::uintptr_t GameBase()
{
	static std::uintptr_t base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
	return base;
}

std::uintptr_t Rel(std::uintptr_t absAddr)
{
	return GameBase() + (absAddr - kPreferredBase);
}

template <typename R, typename T, typename... Args>
R ThisCall(std::uintptr_t addr, T* self, Args... args)
{
	using Fn = R(__thiscall*)(T*, Args...);
	return reinterpret_cast<Fn>(addr)(self, args...);
}

float BitsToFloat(std::uint32_t bits)
{
	float f = 0.f;
	std::memcpy(&f, &bits, sizeof(f));
	return f;
}

bool ValidUserPtr(const void* p)
{
	const auto v = reinterpret_cast<std::uintptr_t>(p);
	return v > 0x10000 && v < 0xFFF00000;
}

float CallAvEaxBits(void* fn, void* self, std::uint32_t code)
{
	if (!fn) return 0.f;
	using FnU = std::uint32_t(__thiscall*)(void*, std::uint32_t);
	return BitsToFloat(reinterpret_cast<FnU>(fn)(self, code));
}

float CallAvSt0(void* fn, void* self, std::uint32_t code)
{
	if (!fn) return 0.f;
	using FnF = float(__thiscall*)(void*, std::uint32_t);
	return reinterpret_cast<FnF>(fn)(self, code);
}

bool AvLooksOk(float v, float maxHint, bool allowZero)
{
	if (!std::isfinite(v)) return false;
	if (v < 0.f) return false;
	if (!allowZero && v <= 0.f) return false;
	if (maxHint > 1.f && v > maxHint * 2.5f) return false;
	if (v > 100000.f) return false;
	return true;
}

struct FormsMap {
	void** vtbl;
	std::uint32_t numBuckets;
	struct Entry {
		Entry* next;
		std::uint32_t key;
		void* data;
	}** buckets;
	std::uint32_t numItems;
};

void* LookupFormMap(std::uint32_t formId)
{
	__try {
		FormsMap** slot = reinterpret_cast<FormsMap**>(Rel(kFormsMapAbs));
		FormsMap* map = slot ? *slot : nullptr;
		if (!map || !map->buckets || map->numBuckets == 0) return nullptr;
		for (auto* e = map->buckets[formId % map->numBuckets]; e; e = e->next) {
			if (e->key == formId) return e->data;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
	return nullptr;
}

void* LookupForm(std::uint32_t formId)
{
	if (void* f = LookupFormMap(formId)) return f;
	__try {
		using Fn = void*(__cdecl*)(std::uint32_t);
		return reinterpret_cast<Fn>(Rel(kLookupFormAbs))(formId);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

void* LookupEditor(const char* edid)
{
	__try {
		using Fn = void*(__cdecl*)(const char*);
		return reinterpret_cast<Fn>(Rel(kGetFormByEditorIdAbs))(edid);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

const char* ReadStringData(void* stringObj)
{
	if (!stringObj) return nullptr;
	char* data = *reinterpret_cast<char**>(stringObj);
	if (!data) return nullptr;
	__try {
		if (data[0]) return data;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
	return nullptr;
}

const char* ReadFullNameComponent(void* fullNameComp)
{
	// TESFullName: vtbl + String name at +4
	return ReadStringData(reinterpret_cast<char*>(fullNameComp) + 4);
}

std::uint32_t FormRefId(void* form)
{
	if (!form) return 0;
	return *reinterpret_cast<std::uint32_t*>(reinterpret_cast<char*>(form) + kOff_FormRefID);
}

std::uint8_t FormTypeId(void* form)
{
	if (!form) return 0;
	return *reinterpret_cast<std::uint8_t*>(reinterpret_cast<char*>(form) + kOff_FormTypeID);
}

float ReadGlobalFloat(std::uint32_t formId, const char* edidFallback)
{
	void* form = LookupForm(formId);
	if (!form && edidFallback) form = LookupEditor(edidFallback);
	if (!form) return -1.f;
	// TESGlobal::data is always a float at +0x24 (type byte only affects display).
	float v = *reinterpret_cast<float*>(reinterpret_cast<char*>(form) + 0x24);
	if (!std::isfinite(v)) return -1.f;
	// Normalize into 0..24 for clock display (engine can drift slightly).
	if (v < 0.f) return -1.f;
	if (v >= 24.f) {
		v = std::fmod(v, 24.f);
		if (v < 0.f) v += 24.f;
	}
	return v;
}

bool IsLootBaseType(std::uint8_t t)
{
	return t == kFormType_TESObjectWEAP
		|| t == kFormType_TESAmmo
		|| t == kFormType_TESObjectMISC
		|| t == kFormType_AlchemyItem
		|| t == kFormType_TESObjectARMO
		|| t == kFormType_TESObjectBOOK
		|| t == kFormType_TESKey
		|| t == kFormType_BGSNote;
}

const char* NameForRef(void* refr); // defined below

struct ActorMarkPod {
	float x, y, z, dist;
	std::uint32_t refId;
	const char* name;
};

bool TryActorMarkPod(void* actor, void* player, float maxDist, ActorMarkPod* out)
{
	if (!actor || !out || actor == player) return false;
	__try {
		float px, py, pz, x, y, z;
		if (!ReadRefWorldPos(player, &px, &py, &pz)) return false;
		if (!ReadRefWorldPos(actor, &x, &y, &z)) return false;
		const float dx = x - px, dy = y - py, dz = z - pz;
		const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (!std::isfinite(dist) || dist > maxDist) return false;
		out->x = x; out->y = y; out->z = z; out->dist = dist;
		out->refId = FormRefId(actor);
		out->name = NameForRef(actor);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void AppendActorMarker(void* actor, void* player, float maxDist, std::vector<NearbyMarker>& tmp)
{
	ActorMarkPod pod{};
	if (!TryActorMarkPod(actor, player, maxDist, &pod)) return;
	NearbyMarker m;
	m.kind = 0;
	m.distance = pod.dist;
	m.x = pod.x; m.y = pod.y; m.z = pod.z;
	m.refId = pod.refId;
	m.name = pod.name ? pod.name : "NPC";
	tmp.push_back(std::move(m));
}

void WalkActorList(void* listObj, void* player, float maxDist, std::vector<NearbyMarker>& tmp)
{
	// tList uses an embedded dummy head: { item=null, next=first }.
	struct Node { void* actor; Node* next; };
	Node* n = nullptr;
	__try {
		n = reinterpret_cast<Node*>(listObj);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return;
	}
	for (int guard = 0; n && guard < 4096; ++guard) {
		void* actor = nullptr;
		Node* next = nullptr;
		__try {
			actor = n->actor;
			next = n->next;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			break;
		}
		if (actor) AppendActorMarker(actor, player, maxDist, tmp);
		n = next;
	}
}

void* ReadPlayerCell(void* player)
{
	__try {
		return *reinterpret_cast<void**>(reinterpret_cast<char*>(player) + kOff_ParentCell);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

bool ReadPlayerPos(void* player, float* px, float* py, float* pz)
{
	return ReadRefWorldPos(player, px, py, pz);
}

struct CellNode { void* refr; CellNode* next; };

CellNode* ReadCellListHead(void* cell)
{
	__try {
		return reinterpret_cast<CellNode*>(reinterpret_cast<char*>(cell) + kOff_CellObjectList);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

bool ReadCellNode(CellNode* n, void** refrOut, CellNode** nextOut)
{
	__try {
		*refrOut = n->refr;
		*nextOut = n->next;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool ReadCellGridXY(void* cell, std::int32_t* ox, std::int32_t* oy)
{
	__try {
		if (!ValidUserPtr(cell) || !ox || !oy) return false;
		void* data = *reinterpret_cast<void**>(reinterpret_cast<char*>(cell) + kOff_CellData);
		if (!ValidUserPtr(data)) return false;
		*ox = *reinterpret_cast<std::int32_t*>(data);
		*oy = *reinterpret_cast<std::int32_t*>(reinterpret_cast<char*>(data) + 4);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void* ReadCellWorldSpace(void* cell)
{
	__try {
		if (!ValidUserPtr(cell)) return nullptr;
		void* ws = *reinterpret_cast<void**>(reinterpret_cast<char*>(cell) + kOff_CellWorldSpace);
		return ValidUserPtr(ws) ? ws : nullptr;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

// NiTPointerMap-style bucket walk (Gamebryo). Key matches NVSE CellScanInfo packing.
void* LookupWorldCell(void* worldSpace, std::int32_t cx, std::int32_t cy)
{
	__try {
		if (!ValidUserPtr(worldSpace)) return nullptr;
		void* map = *reinterpret_cast<void**>(reinterpret_cast<char*>(worldSpace) + kOff_WsCellMap);
		if (!ValidUserPtr(map)) return nullptr;

		const std::uint32_t numBuckets = *reinterpret_cast<std::uint32_t*>(reinterpret_cast<char*>(map) + 0x04);
		void** buckets = *reinterpret_cast<void***>(reinterpret_cast<char*>(map) + 0x08);
		if (!buckets || numBuckets == 0 || numBuckets > 0x10000) return nullptr;

		const std::uint32_t key = (static_cast<std::uint32_t>(cx) << 16)
			+ static_cast<std::uint32_t>((cy << 16) >> 16);
		const std::uint32_t bucket = key % numBuckets;
		struct Entry { Entry* next; std::uint32_t key; void* data; };
		Entry* e = reinterpret_cast<Entry*>(buckets[bucket]);
		for (int guard = 0; e && guard < 4096; ++guard) {
			Entry* cur = e;
			e = e->next;
			if (cur->key == key && ValidUserPtr(cur->data))
				return cur->data;
		}
		return nullptr;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

struct CellMarkPod {
	std::uint8_t kind;
	float x, y, z, dist;
	std::uint32_t refId;
	const char* name;
};

bool TryCellMarkPod(void* refr, void* player, float px, float py, float pz, float maxDist,
	bool npcs, bool loot, bool doors, CellMarkPod* out)
{
	if (!refr || !out || refr == player) return false;
	__try {
		const auto rt = FormTypeId(refr);
		std::uint8_t kind = 255;
		if (npcs && (rt == kFormType_Character || rt == kFormType_Creature)) {
			kind = 0;
		} else {
			void* base = *reinterpret_cast<void**>(reinterpret_cast<char*>(refr) + kOff_BaseForm);
			if (!base) return false;
			const auto bt = FormTypeId(base);
			if (doors && bt == kFormType_TESObjectDOOR) kind = 2;
			else if (loot && bt == kFormType_TESObjectCONT) kind = 3;
			else if (loot && IsLootBaseType(bt)) kind = 1;
			else return false;
		}
		if (kind == 255) return false;

		float x, y, z;
		if (!ReadRefWorldPos(refr, &x, &y, &z)) return false;
		const float dx = x - px, dy = y - py, dz = z - pz;
		const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (!std::isfinite(dist) || dist > maxDist) return false;

		out->kind = kind;
		out->x = x; out->y = y; out->z = z; out->dist = dist;
		out->refId = FormRefId(refr);
		out->name = NameForRef(refr);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void ScanOneCellObjects(void* cell, void* player, float px, float py, float pz, float maxDist,
	bool npcs, bool loot, bool doors, std::vector<NearbyMarker>& tmp)
{
	if (!cell) return;
	CellNode* n = ReadCellListHead(cell);
	for (int guard = 0; n && guard < 8192; ++guard) {
		void* refr = nullptr;
		CellNode* next = nullptr;
		if (!ReadCellNode(n, &refr, &next)) break;
		n = next;
		CellMarkPod pod{};
		if (!TryCellMarkPod(refr, player, px, py, pz, maxDist, npcs, loot, doors, &pod)) continue;
		NearbyMarker m;
		m.kind = pod.kind;
		m.distance = pod.dist;
		m.x = pod.x; m.y = pod.y; m.z = pod.z;
		m.refId = pod.refId;
		if (pod.name) m.name = pod.name;
		else m.name = pod.kind == 0 ? "NPC" : (pod.kind == 2 ? "Door" : (pod.kind == 3 ? "Container" : "Loot"));
		tmp.push_back(std::move(m));
	}
}

void ScanCellObjects(void* player, float maxDist, bool npcs, bool loot, bool doors,
	std::vector<NearbyMarker>& tmp)
{
	void* cell = ReadPlayerCell(player);
	if (!cell || (!loot && !doors && !npcs)) return;
	float px = 0, py = 0, pz = 0;
	if (!ReadPlayerPos(player, &px, &py, &pz)) return;

	// Always scan current cell (interiors + exterior).
	ScanOneCellObjects(cell, player, px, py, pz, maxDist, npcs, loot, doors, tmp);

	// Exterior: also walk neighboring LOADED cells so loot isn't limited to one 4096u tile.
	// Cell size ≈ 4096 units. Depth 2 ≈ 5×5 grid (what uGrids typically keeps hot).
	void* world = ReadCellWorldSpace(cell);
	std::int32_t originX = 0, originY = 0;
	if (!world || !ReadCellGridXY(cell, &originX, &originY)) return;

	int depth = static_cast<int>(std::ceil(maxDist / 4096.f));
	if (depth < 1) depth = 1;
	if (depth > 5) depth = 5; // loaded uGrids neighborhood; unloaded Lookup returns null

	for (int dy = -depth; dy <= depth; ++dy) {
		for (int dx = -depth; dx <= depth; ++dx) {
			if (dx == 0 && dy == 0) continue; // already scanned
			void* other = LookupWorldCell(world, originX + dx, originY + dy);
			if (!other || other == cell) continue;
			ScanOneCellObjects(other, player, px, py, pz, maxDist, npcs, loot, doors, tmp);
		}
	}
}

void ScanNearbyImpl(void* player, float maxDist, int maxMarkers, bool npcs, bool loot, bool doors,
	std::vector<NearbyMarker>& out)
{
	out.clear();
	std::vector<NearbyMarker> tmp;
	tmp.reserve(64);

	if (npcs) {
		auto* mgr = reinterpret_cast<char*>(Rel(kActorProcessMgrAbs));
		WalkActorList(mgr + kOff_HighActors, player, maxDist, tmp);
		WalkActorList(mgr + kOff_MiddleHighActors, player, maxDist, tmp);
	}

	ScanCellObjects(player, maxDist, npcs, loot, doors, tmp);

	std::sort(tmp.begin(), tmp.end(), [](const NearbyMarker& a, const NearbyMarker& b) {
		if (a.distance != b.distance) return a.distance < b.distance;
		return a.name < b.name;
	});
	std::vector<NearbyMarker> dedup;
	dedup.reserve(tmp.size());
	for (auto& m : tmp) {
		bool skip = false;
		for (const auto& d : dedup) {
			if (m.refId != 0 && d.refId == m.refId) {
				skip = true;
				break;
			}
			if (m.refId == 0 && d.kind == m.kind && d.name == m.name && std::fabs(d.distance - m.distance) < 2.f) {
				skip = true;
				break;
			}
		}
		if (!skip) dedup.push_back(std::move(m));
	}

	if (static_cast<int>(dedup.size()) > maxMarkers) dedup.resize(static_cast<size_t>(maxMarkers));
	out = std::move(dedup);
}

int CountFromEntry(void* entry)
{
	// EntryData: extendData@0, countDelta@4, type@8
	const int delta = *reinterpret_cast<std::int32_t*>(reinterpret_cast<char*>(entry) + 0x04);
	void* extendHead = *reinterpret_cast<void**>(entry);
	if (!extendHead) return delta > 0 ? delta : 0;

	// Prefer summing ExtraCount (0x24) stacks when present — more reliable than delta alone.
	struct XNode { void* xlist; XNode* next; };
	int stackSum = 0, stacks = 0, guard = 0;
	for (XNode* n = reinterpret_cast<XNode*>(extendHead); n && guard < 256; n = n->next, ++guard) {
		if (!n->xlist) continue;
		++stacks;
		void* xCount = ThisCall<void*>(Rel(kExtraGetByTypeAbs), n->xlist, static_cast<std::uint8_t>(0x24));
		if (xCount) {
			stackSum += *reinterpret_cast<std::int16_t*>(reinterpret_cast<char*>(xCount) + 0x0C);
		} else {
			stackSum += 1;
		}
	}
	if (stacks > 0 && stackSum > 0) return stackSum;
	return delta > 0 ? delta : 0;
}

int CountCaps(void* player)
{
	if (!player) return -1;
	void* capsForm = LookupForm(kFormCaps001);
	if (!capsForm) capsForm = LookupEditor("Caps001");

	void* extraList = reinterpret_cast<char*>(player) + kOff_ExtraList;
	void* xChanges = ThisCall<void*>(Rel(kExtraGetByTypeAbs), extraList, static_cast<std::uint8_t>(kExtraContainerChanges));
	if (!xChanges) return 0;

	void* data = *reinterpret_cast<void**>(reinterpret_cast<char*>(xChanges) + 0x0C);
	if (!data) return 0;
	void* listObj = *reinterpret_cast<void**>(data);
	if (!listObj) return 0;

	struct Node { void* entry; Node* next; };
	int total = 0, guard = 0;
	for (Node* n = reinterpret_cast<Node*>(listObj); n && guard < 4096; n = n->next, ++guard) {
		if (!n->entry) continue;
		void* type = *reinterpret_cast<void**>(reinterpret_cast<char*>(n->entry) + 0x08);
		if (!type) continue;
		const std::uint32_t id = FormRefId(type);
		const bool match = (id == kFormCaps001) || (capsForm && type == capsForm);
		if (!match) continue;
		total += CountFromEntry(n->entry);
	}
	return total < 0 ? 0 : total;
}

void ReadCellName(void* player, char* out, size_t outLen)
{
	out[0] = 0;
	void* cell = *reinterpret_cast<void**>(reinterpret_cast<char*>(player) + kOff_ParentCell);
	if (!cell) return;
	// TESObjectCELL::fullName at +0x18 → String at +0x1C
	const char* name = ReadFullNameComponent(reinterpret_cast<char*>(cell) + 0x18);
	if (name) strncpy_s(out, outLen, name, _TRUNCATE);
}

// Minimal AV path only — Fn_01/Fn_06 (ST0) + Fn_08 (EAX bits).
// Do NOT CallAvSt0 on Fn_03 (returns EAX; that unbalances the FPU and crashes later).
// No C++ objects / lambdas inside __try (MSVC SEH rule).
float ReadCurrentAv(void* av, void* fn01, void* fn06, std::uint32_t code, float maxHint)
{
	float cur = 0.f;
	if (fn01 && fn06) {
		cur = CallAvSt0(fn01, av, code) + CallAvSt0(fn06, av, code);
		if (AvLooksOk(cur, maxHint, true)) return cur;
	}
	if (maxHint > 1.f) return maxHint;
	return 0.f;
}

bool ReadActorValues(void* player, float& health, float& healthMax, float& ap, float& apMax, int& level)
{
	struct AVOwner { void** vtbl; };
	auto* av = reinterpret_cast<AVOwner*>(reinterpret_cast<char*>(player) + kOff_AVOwner);
	if (!av || !av->vtbl) return false;

	health = healthMax = ap = apMax = 0.f;
	level = 0;

	__try {
		void* fn01 = av->vtbl[1];
		void* fn06 = av->vtbl[6];
		void* fn08 = av->vtbl[8];
		void* fn0A = av->vtbl[0xA]; // GetLevel — documented in NVSE ActorValueOwner

		healthMax = CallAvEaxBits(fn08, av, kAV_Health);
		apMax = CallAvEaxBits(fn08, av, kAV_ActionPoints);
		if (!AvLooksOk(healthMax, 0.f, false)) healthMax = CallAvSt0(fn01, av, kAV_Health);
		if (!AvLooksOk(apMax, 0.f, false)) apMax = CallAvSt0(fn01, av, kAV_ActionPoints);

		health = ReadCurrentAv(av, fn01, fn06, kAV_Health, healthMax);
		ap = ReadCurrentAv(av, fn01, fn06, kAV_ActionPoints, apMax);

		if (fn0A) {
			using GetLevelFn = std::uint16_t(__thiscall*)(void*);
			level = static_cast<int>(reinterpret_cast<GetLevelFn>(fn0A)(av));
			if (level < 0 || level > 1000) level = 0;
		}
		return AvLooksOk(healthMax, 0.f, false) || AvLooksOk(apMax, 0.f, false) || health > 0.f;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void ReadWeapon(void* player, char* nameOut, size_t nameLen, int& clip, int& clipMax, int& reserve)
{
	nameOut[0] = 0;
	clip = clipMax = reserve = -1;

	void* proc = *reinterpret_cast<void**>(reinterpret_cast<char*>(player) + kOff_BaseProcess);
	if (!proc) {
		strncpy_s(nameOut, nameLen, "Unarmed", _TRUNCATE);
		return;
	}

	void* weap = nullptr;
	void* ammoInfo = nullptr;

	__try {
		// Prefer virtual GetWeaponInfo (returns EntryData* / WeaponInfo* — type/weapon at +0x08)
		void** vtbl = *reinterpret_cast<void***>(proc);
		if (vtbl) {
			// BaseProcess::GetWeaponInfo is virtual ~0x52 (see xNVSE GameProcess.h)
			using GetWepFn = void*(__thiscall*)(void*);
			using GetAmmoFn = void*(__thiscall*)(void*);
			if (vtbl[0x52]) {
				void* info = reinterpret_cast<GetWepFn>(vtbl[0x52])(proc);
				if (info) weap = *reinterpret_cast<void**>(reinterpret_cast<char*>(info) + 0x08);
			}
			if (vtbl[0x53]) {
				ammoInfo = reinterpret_cast<GetAmmoFn>(vtbl[0x53])(proc);
			}
		}
		// Fallback: MiddleHighProcess member pointers
		if (!weap) {
			void* weaponInfo = *reinterpret_cast<void**>(reinterpret_cast<char*>(proc) + kOff_WeaponInfo);
			if (weaponInfo) weap = *reinterpret_cast<void**>(reinterpret_cast<char*>(weaponInfo) + 0x08);
		}
		if (!ammoInfo) {
			ammoInfo = *reinterpret_cast<void**>(reinterpret_cast<char*>(proc) + kOff_AmmoInfo);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		weap = nullptr;
	}

	if (!weap) {
		strncpy_s(nameOut, nameLen, "Unarmed", _TRUNCATE);
		return;
	}

	const char* nm = ReadFullNameComponent(reinterpret_cast<char*>(weap) + kOff_WeapFullName);
	if (nm) strncpy_s(nameOut, nameLen, nm, _TRUNCATE);
	else strncpy_s(nameOut, nameLen, "Weapon", _TRUNCATE);

	clipMax = static_cast<int>(*reinterpret_cast<std::uint8_t*>(reinterpret_cast<char*>(weap) + kOff_WeapClipRounds));
	if (ammoInfo) {
		clip = static_cast<int>(*reinterpret_cast<std::uint32_t*>(reinterpret_cast<char*>(ammoInfo) + 0x04));
		void* ammoForm = *reinterpret_cast<void**>(reinterpret_cast<char*>(ammoInfo) + 0x08);
		if (ammoForm) {
			void* extraList = reinterpret_cast<char*>(player) + kOff_ExtraList;
			void* xChanges = ThisCall<void*>(Rel(kExtraGetByTypeAbs), extraList, static_cast<std::uint8_t>(kExtraContainerChanges));
			if (xChanges) {
				void* data = *reinterpret_cast<void**>(reinterpret_cast<char*>(xChanges) + 0x0C);
				void* listObj = data ? *reinterpret_cast<void**>(data) : nullptr;
				struct Node { void* entry; Node* next; };
				int total = 0, guard = 0;
				for (Node* n = reinterpret_cast<Node*>(listObj); n && guard < 4096; n = n->next, ++guard) {
					if (!n->entry) continue;
					void* type = *reinterpret_cast<void**>(reinterpret_cast<char*>(n->entry) + 0x08);
					if (type != ammoForm) continue;
					total += CountFromEntry(n->entry);
				}
				if (total >= 0) reserve = total;
			}
		}
	}
}

const char* NameForRef(void* refr)
{
	__try {
		if (!refr) return nullptr;
		void* base = *reinterpret_cast<void**>(reinterpret_cast<char*>(refr) + kOff_BaseForm);
		if (!base) return nullptr;
		const auto t = FormTypeId(base);
		const char* n = nullptr;
		if (t == kFormType_TESNPC || t == kFormType_TESCreature)
			n = ReadFullNameComponent(reinterpret_cast<char*>(base) + kOff_ActorBaseFullName);
		else
			n = ReadFullNameComponent(reinterpret_cast<char*>(base) + kOff_ContFullName);
		if (n && n[0] && !(n[0] == '?' && n[1] == '\0')) return n;
		// Fallback: try TESFullName at 0x18 (some bound objects)
		n = ReadFullNameComponent(reinterpret_cast<char*>(base) + 0x18);
		if (n && n[0] && !(n[0] == '?' && n[1] == '\0')) return n;
		return nullptr;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

struct PodSnap {
	int valid = 0;
	int stage = 0;
	int healthStatus = 0; // 0 unavail 1 invalid 2 valid
	int capsStatus = 0;
	int weaponStatus = 0;
	float health = 0, healthMax = 0, ap = 0, apMax = 0;
	int level = 0;
	int caps = -1;
	float gameHour = -1.f;
	float posX = 0, posY = 0, posZ = 0;
	int hasPos = 0;
	char location[96]{};
	char weapon[64]{};
	int ammoClip = -1, ammoClipMax = -1, ammoReserve = -1;
};

PodSnap RefreshPod(bool includeHeavy)
{
	PodSnap s{};
	s.stage = 1;
	s.healthStatus = 0;
	s.capsStatus = 0;
	s.weaponStatus = 0;
	__try {
		void** slot = reinterpret_cast<void**>(Rel(kThePlayerAbs));
		void* player = slot ? *slot : nullptr;
		s.stage = 2;
		if (!player) { s.stage = 3; return s; }

		s.stage = 4;
		if (!ReadActorValues(player, s.health, s.healthMax, s.ap, s.apMax, s.level)) {
			s.stage = 5;
			s.healthStatus = 1;
			return s;
		}
		s.healthStatus = 2;

		s.valid = 1;
		if (ReadRefWorldPos(player, &s.posX, &s.posY, &s.posZ)) {
			s.hasPos = 1;
		} else {
			SFC_ERR("[GAME_READY] player XYZ unavailable — overlay hard-disabled");
			HardDisableOverlayDraws("player_xyz_nan");
			s.hasPos = 0;
			s.posX = s.posY = s.posZ = 0.f;
			s.valid = 0;
			s.stage = 6;
			return s;
		}
		if (!std::isfinite(s.posX) || !std::isfinite(s.posY) || !std::isfinite(s.posZ)) {
			SFC_ERR("[GAME_READY] player XYZ became NaN/Inf — overlay hard-disabled");
			HardDisableOverlayDraws("player_xyz_nan");
			s.hasPos = 0;
			s.posX = s.posY = s.posZ = 0.f;
			s.valid = 0;
			s.stage = 6;
			return s;
		}
		s.hasPos = 1;
		s.stage = 7;

		// Light path ends here — caps/cell/weapon were correlating with freezes after load.
		if (!includeHeavy) {
			s.capsStatus = 0;
			s.weaponStatus = 0;
			s.stage = 11;
			return s;
		}

		__try { s.gameHour = ReadGlobalFloat(kFormGameHour, "GameHour"); }
		__except (EXCEPTION_EXECUTE_HANDLER) { s.gameHour = -1.f; }

		s.stage = 8;
		__try {
			s.caps = CountCaps(player);
			s.capsStatus = (s.caps >= 0) ? 2 : 1;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			s.caps = -1;
			s.capsStatus = 1;
		}

		s.stage = 9;
		ReadCellName(player, s.location, sizeof(s.location));

		s.stage = 10;
		__try {
			ReadWeapon(player, s.weapon, sizeof(s.weapon), s.ammoClip, s.ammoClipMax, s.ammoReserve);
			s.weaponStatus = 2;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			s.weaponStatus = 1;
		}
		s.stage = 11;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		if (!s.valid) {
			const int st = s.stage;
			const int hs = s.healthStatus;
			s = PodSnap{};
			s.stage = st;
			s.healthStatus = hs ? hs : 1;
		}
	}
	return s;
}

bool ScanNearbySafe(void* player, float maxDist, int maxMarkers, bool npcs, bool loot, bool doors,
	std::vector<NearbyMarker>& out)
{
	// Per-node SEH lives inside ScanNearbyImpl. Outer catch is last-resort only —
	// do NOT clear `out` here; caller keeps the previous good snapshot on failure.
	__try {
		ScanNearbyImpl(player, maxDist, maxMarkers, npcs, loot, doors, out);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

PodSnap RefreshPodSealed(bool includeHeavy)
{
	PodSnap pod{};
	__try {
		pod = RefreshPod(includeHeavy);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		pod = PodSnap{};
		pod.stage = -1;
		pod.healthStatus = 1;
	}
	return pod;
}

} // namespace

GameState& GameState::Get()
{
	static GameState instance;
	return instance;
}

void GameState::Invalidate(const char* reason)
{
	// Keep last good HUD numbers during door settle — only mark unavailable.
	snap_.status = ReadStatus::Unavailable;
	snap_.valid = false;
	refreshAccum_ = 0.f;
	weaponAccum_ = 0.f;
	heavyAccum_ = 0.f;
	lightOkStreak_ = 0;
	if (reason)
		DiagThrottle("GameState.invalidate", 750, "%s", reason);
}

void GameState::NoteExteriorStream()
{
	// Hold off AV vtable calls for ~2.5s while the world streams; keep last HP on HUD.
	streamCooldown_ = 2.5f;
	DiagThrottle("GameState.stream_pause", 2000, "exterior_stream");
}

void GameState::Tick(float dt)
{
	// playable-v13: meters for HP/AP %; ActionPoints text "12/5" is ammo (not AP).
	refreshAccum_ += dt;
	if (refreshAccum_ < 0.35f) return;
	refreshAccum_ = 0.f;

	UiHudDumpHitPointsOnce();

	UiHudStats ui{};
	if (!TryReadUiHudStats(ui) || !ui.ok) {
		DiagThrottle("GameState.ui.miss", 5000, "no HUD traits yet");
		return;
	}

	PlayerSnapshot s = snap_;
	s.valid = true;
	s.status = ReadStatus::Valid;
	s.stage = 11;

	// HP/AP are bar percentages from vanilla meters (absolute AV reads crash).
	if (ui.hpFill >= 0.f) {
		s.healthStatus = ReadStatus::Valid;
		s.health = ui.hpFill * 100.f;
		s.healthMax = 100.f;
	}
	if (ui.apFill >= 0.f) {
		s.ap = ui.apFill * 100.f;
		s.apMax = 100.f;
	}

	if (ui.ammoClip >= 0) {
		s.weaponStatus = ReadStatus::Valid;
		s.ammoClip = ui.ammoClip;
		s.ammoReserve = ui.ammoReserve;
		if (ui.ammoText[0]) s.weaponName = ui.ammoText;
		else {
			char buf[32]{};
			std::snprintf(buf, sizeof(buf), "%d/%d", ui.ammoClip, ui.ammoReserve);
			s.weaponName = buf;
		}
	} else if (ui.ammoText[0]) {
		s.weaponStatus = ReadStatus::Valid;
		s.weaponName = ui.ammoText;
	}

	if (ui.location[0])
		s.location = ui.location;
	// else keep previous location (Region_Location is often blank)

	snap_ = std::move(s);
	DiagThrottle("GameState.ui.ok", 5000, "hp=%.0f%% ap=%.0f%% ammo=%s loc=%s",
		snap_.health, snap_.ap,
		snap_.weaponName.empty() ? "-" : snap_.weaponName.c_str(),
		snap_.location.empty() ? "-" : snap_.location.c_str());
}

void GameState::ScanNearby(float maxDist, int maxMarkers, bool npcs, bool loot, bool doors)
{
	if (!readsEnabled_) {
		snap_.nearby.clear();
		return;
	}
	// Explicit disable / clear request from ESP when turned off.
	if (maxMarkers <= 0 || maxDist <= 0.f || (!npcs && !loot && !doors)) {
		snap_.nearby.clear();
		return;
	}

	void** slot = reinterpret_cast<void**>(Rel(kThePlayerAbs));
	void* player = slot ? *slot : nullptr;
	if (!player) {
		// Keep last markers — player pointer can flicker during loads.
		return;
	}
	std::vector<NearbyMarker> markers;
	if (!ScanNearbySafe(player, maxDist, maxMarkers, npcs, loot, doors, markers)) {
		SFC_WARN("ESP scan fault — keeping last markers=%d",
			static_cast<int>(snap_.nearby.size()));
		return;
	}
	snap_.nearby = std::move(markers);
}

void GameState::WantNearbyScan(float maxDist, int maxMarkers, bool npcs, bool loot, bool doors)
{
	if (maxDist <= 0.f || maxMarkers <= 0) return;
	if (!npcs && !loot && !doors) return;
	nearbyWantPending_ = true;
	if (maxDist > nearbyWantDist_) nearbyWantDist_ = maxDist;
	if (maxMarkers > nearbyWantMarkers_) nearbyWantMarkers_ = maxMarkers;
	nearbyWantNpcs_ = nearbyWantNpcs_ || npcs;
	nearbyWantLoot_ = nearbyWantLoot_ || loot;
	nearbyWantDoors_ = nearbyWantDoors_ || doors;
}

void GameState::FlushNearbyScanNow()
{
	if (!nearbyWantPending_) return;
	const float dist = nearbyWantDist_;
	const int markers = nearbyWantMarkers_;
	const bool npcs = nearbyWantNpcs_;
	const bool loot = nearbyWantLoot_;
	const bool doors = nearbyWantDoors_;
	nearbyWantPending_ = false;
	nearbyWantDist_ = 0.f;
	nearbyWantMarkers_ = 0;
	nearbyWantNpcs_ = nearbyWantLoot_ = nearbyWantDoors_ = false;
	ScanNearby(dist, markers, npcs, loot, doors);
}

void GameState::FlushNearbyScan(float dt)
{
	if (!nearbyWantPending_) {
		nearbyFlushAccum_ = 0.f;
		return;
	}
	nearbyFlushAccum_ += dt;
	int intervalMs = Config::Get().Data().performance.espScanMs;
	if (intervalMs < 50) intervalMs = 50;
	if (intervalMs > 2000) intervalMs = 2000;
	if (nearbyFlushAccum_ < static_cast<float>(intervalMs) * 0.001f)
		return;
	nearbyFlushAccum_ = 0.f;
	FlushNearbyScanNow();
}

} // namespace sfc
