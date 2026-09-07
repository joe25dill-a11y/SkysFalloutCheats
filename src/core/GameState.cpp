#include "core/GameState.hpp"
#include "core/Log.hpp"
#include "core/Diag.hpp"
#include "render/D3D9Hook.hpp"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cmath>
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

constexpr std::uint8_t kFormType_TESObjectCONT = 0x1B;
constexpr std::uint8_t kFormType_TESObjectDOOR = 0x1C;
constexpr std::uint8_t kFormType_TESObjectMISC = 0x1F;
constexpr std::uint8_t kFormType_TESObjectWEAP = 0x28;
constexpr std::uint8_t kFormType_TESAmmo = 0x29;
constexpr std::uint8_t kFormType_TESNPC = 0x2A;
constexpr std::uint8_t kFormType_TESCreature = 0x2B;
constexpr std::uint8_t kFormType_AlchemyItem = 0x2F;
constexpr std::uint8_t kFormType_Character = 0x3B;
constexpr std::uint8_t kFormType_Creature = 0x3C;

constexpr std::uintptr_t kActorProcessMgrAbs = 0x011E0E80;
constexpr std::ptrdiff_t kOff_HighActors = 0x05C;
constexpr std::ptrdiff_t kOff_MiddleHighActors = 0x000;

constexpr std::ptrdiff_t kOff_PosX = 0x030;
constexpr std::ptrdiff_t kOff_ParentCell = 0x040;
constexpr std::ptrdiff_t kOff_ExtraList = 0x044;
constexpr std::ptrdiff_t kOff_BaseForm = 0x020;
constexpr std::ptrdiff_t kOff_AVOwner = 0x0A4;
constexpr std::ptrdiff_t kOff_BaseProcess = 0x068;
constexpr std::ptrdiff_t kOff_WeaponInfo = 0x114;
constexpr std::ptrdiff_t kOff_AmmoInfo = 0x118;
constexpr std::ptrdiff_t kOff_CellObjectList = 0x0AC;
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
		|| t == kFormType_AlchemyItem;
}

const char* NameForRef(void* refr); // defined below

void AppendActorMarker(void* actor, void* player, float maxDist, std::vector<NearbyMarker>& tmp)
{
	if (!actor || actor == player) return;
	const float px = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX);
	const float py = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX + 4);
	const float pz = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX + 8);
	const float x = *reinterpret_cast<float*>(reinterpret_cast<char*>(actor) + kOff_PosX);
	const float y = *reinterpret_cast<float*>(reinterpret_cast<char*>(actor) + kOff_PosX + 4);
	const float z = *reinterpret_cast<float*>(reinterpret_cast<char*>(actor) + kOff_PosX + 8);
	const float dx = x - px, dy = y - py, dz = z - pz;
	const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
	if (!std::isfinite(dist) || dist > maxDist) return;

	NearbyMarker m;
	m.kind = 0;
	m.distance = dist;
	m.x = x; m.y = y; m.z = z;
	if (const char* nm = NameForRef(actor)) m.name = nm;
	else m.name = "NPC";
	tmp.push_back(std::move(m));
}

void WalkActorList(void* listObj, void* player, float maxDist, std::vector<NearbyMarker>& tmp)
{
	struct Node { void* actor; Node* next; };
	int guard = 0;
	for (Node* n = reinterpret_cast<Node*>(listObj); n && guard < 4096; n = n->next, ++guard) {
		if (!n->actor) continue;
		AppendActorMarker(n->actor, player, maxDist, tmp);
	}
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

bool ReadActorValues(void* player, float& health, float& healthMax, float& ap, float& apMax, int& level)
{
	struct AVOwner { void** vtbl; };
	auto* av = reinterpret_cast<AVOwner*>(reinterpret_cast<char*>(player) + kOff_AVOwner);
	if (!av || !av->vtbl) return false;

	__try {
		void* fn01 = av->vtbl[1]; // base ST0
		void* fn02 = av->vtbl[2]; // current EAX bits
		void* fn03 = av->vtbl[3]; // current EAX bits (Eval)
		void* fn06 = av->vtbl[6]; // damage/mod ST0 (added to base)
		void* fn08 = av->vtbl[8]; // permanent EAX bits
		using GetLevelFn = std::uint16_t(__thiscall*)(AVOwner*);
		auto getLevel = reinterpret_cast<GetLevelFn>(av->vtbl[0xA]);

		healthMax = CallAvEaxBits(fn08, av, kAV_Health);
		apMax = CallAvEaxBits(fn08, av, kAV_ActionPoints);
		if (!AvLooksOk(healthMax, 0.f, false)) healthMax = CallAvSt0(fn01, av, kAV_Health);
		if (!AvLooksOk(apMax, 0.f, false)) apMax = CallAvSt0(fn01, av, kAV_ActionPoints);

		// Current HP/AP: prefer base + damage/mod (NVSE: Fn_06 added to Fn_01).
		// Fn_02/03 often return 0 via our call path; treating 0 as "valid" blocked the fallback.
		auto readCurrent = [&](std::uint32_t code, float maxHint) -> float {
			float cur = 0.f;
			if (fn01 && fn06) {
				cur = CallAvSt0(fn01, av, code) + CallAvSt0(fn06, av, code);
				if (AvLooksOk(cur, maxHint, true) && cur > 0.f) return cur;
			}
			cur = CallAvEaxBits(fn02, av, code);
			if (AvLooksOk(cur, maxHint, true) && cur > 0.f) return cur;
			cur = CallAvEaxBits(fn03, av, code);
			if (AvLooksOk(cur, maxHint, true) && cur > 0.f) return cur;
			// ST0 attempt on Fn_03 (some builds)
			cur = CallAvSt0(fn03, av, code);
			if (AvLooksOk(cur, maxHint, true) && cur > 0.f) return cur;
			// Full health / full AP display fallback when max is known
			if (maxHint > 1.f) return maxHint;
			return 0.f;
		};

		health = readCurrent(kAV_Health, healthMax);
		ap = readCurrent(kAV_ActionPoints, apMax);

		if (getLevel) level = static_cast<int>(getLevel(av));
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
	void* base = *reinterpret_cast<void**>(reinterpret_cast<char*>(refr) + kOff_BaseForm);
	if (!base) return nullptr;
	const auto t = FormTypeId(base);
	if (t == kFormType_TESNPC || t == kFormType_TESCreature)
		return ReadFullNameComponent(reinterpret_cast<char*>(base) + kOff_ActorBaseFullName);
	if (t == kFormType_TESObjectCONT || t == kFormType_TESObjectDOOR || t == kFormType_TESObjectWEAP)
		return ReadFullNameComponent(reinterpret_cast<char*>(base) + kOff_ContFullName);
	return ReadFullNameComponent(reinterpret_cast<char*>(base) + kOff_ContFullName);
}

void ScanNearbyImpl(void* player, float maxDist, int maxMarkers, bool npcs, bool loot, bool doors,
	std::vector<NearbyMarker>& out)
{
	out.clear();
	std::vector<NearbyMarker> tmp;
	tmp.reserve(64);

	if (npcs) {
		// High-process actors (most reliable for living NPCs in the loaded area).
		auto* mgr = reinterpret_cast<char*>(Rel(kActorProcessMgrAbs));
		WalkActorList(mgr + kOff_HighActors, player, maxDist, tmp);
		WalkActorList(mgr + kOff_MiddleHighActors, player, maxDist, tmp);
	}

	void* cell = *reinterpret_cast<void**>(reinterpret_cast<char*>(player) + kOff_ParentCell);
	if (cell && (loot || doors || npcs)) {
		const float px = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX);
		const float py = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX + 4);
		const float pz = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX + 8);

		void* listObj = reinterpret_cast<char*>(cell) + kOff_CellObjectList;
		struct Node { void* refr; Node* next; };
		int guard = 0;
		for (Node* n = reinterpret_cast<Node*>(listObj); n && guard < 8192; n = n->next, ++guard) {
			void* refr = n->refr;
			if (!refr || refr == player) continue;

			const auto rt = FormTypeId(refr);
			std::uint8_t kind = 255;

			if (npcs && (rt == kFormType_Character || rt == kFormType_Creature)) {
				// May already be in process lists — still OK, we'll dedupe by name+dist later if needed.
				kind = 0;
			} else {
				void* base = *reinterpret_cast<void**>(reinterpret_cast<char*>(refr) + kOff_BaseForm);
				if (!base) continue;
				const auto bt = FormTypeId(base);
				if (loot && (bt == kFormType_TESObjectCONT || IsLootBaseType(bt))) kind = 1;
				else if (doors && bt == kFormType_TESObjectDOOR) kind = 2;
				else continue;
			}
			if (kind == 255) continue;

			const float x = *reinterpret_cast<float*>(reinterpret_cast<char*>(refr) + kOff_PosX);
			const float y = *reinterpret_cast<float*>(reinterpret_cast<char*>(refr) + kOff_PosX + 4);
			const float z = *reinterpret_cast<float*>(reinterpret_cast<char*>(refr) + kOff_PosX + 8);
			const float dx = x - px, dy = y - py, dz = z - pz;
			const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (!std::isfinite(dist) || dist > maxDist) continue;

			NearbyMarker m;
			m.kind = kind;
			m.distance = dist;
			m.x = x; m.y = y; m.z = z;
			if (const char* nm = NameForRef(refr)) m.name = nm;
			else m.name = kind == 0 ? "NPC" : (kind == 2 ? "Door" : "Loot");
			tmp.push_back(std::move(m));
		}
	}

	// Dedupe roughly (same name + similar distance).
	std::sort(tmp.begin(), tmp.end(), [](const NearbyMarker& a, const NearbyMarker& b) {
		if (a.distance != b.distance) return a.distance < b.distance;
		return a.name < b.name;
	});
	std::vector<NearbyMarker> dedup;
	dedup.reserve(tmp.size());
	for (auto& m : tmp) {
		bool skip = false;
		for (const auto& d : dedup) {
			if (d.kind == m.kind && d.name == m.name && std::fabs(d.distance - m.distance) < 2.f) {
				skip = true;
				break;
			}
		}
		if (!skip) dedup.push_back(std::move(m));
	}

	if (static_cast<int>(dedup.size()) > maxMarkers) dedup.resize(static_cast<size_t>(maxMarkers));
	out = std::move(dedup);
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

PodSnap RefreshPod(bool includeWeapon)
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
		s.posX = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX);
		s.posY = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX + 4);
		s.posZ = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX + 8);
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
		if (includeWeapon) {
			__try {
				ReadWeapon(player, s.weapon, sizeof(s.weapon), s.ammoClip, s.ammoClipMax, s.ammoReserve);
				s.weaponStatus = 2;
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				s.weaponStatus = 1;
			}
		} else {
			s.weaponStatus = 0; // skipped — keep previous HUD weapon string
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
	__try {
		ScanNearbyImpl(player, maxDist, maxMarkers, npcs, loot, doors, out);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		out.clear();
		return false;
	}
}

PodSnap RefreshPodSealed(bool includeWeapon)
{
	PodSnap pod{};
	__try {
		pod = RefreshPod(includeWeapon);
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

void GameState::Tick(float dt)
{
	if (!readsEnabled_) {
		faultCooldown_ -= dt;
		if (faultCooldown_ <= 0.f) {
			readsEnabled_ = true;
			faultStreak_ = 0;
			SFC_LOG("[GAME_READY] GameState reads re-enabled after cooldown");
		}
		snap_.status = ReadStatus::Unavailable;
		snap_.valid = false;
		return;
	}

	refreshAccum_ += dt;
	weaponAccum_ += dt;
	if (refreshAccum_ < 0.50f) return;
	refreshAccum_ = 0.f;

	// Weapon/process vtable probes are the highest-risk live reads while running/combat.
	// Warm up with light reads first, then refresh weapon at most every 2s.
	constexpr int kWeaponWarmupReads = 8; // ~4s of successful light HUD
	const bool wantWeapon = (lightOkStreak_ >= kWeaponWarmupReads) && (weaponAccum_ >= 2.0f);
	if (wantWeapon)
		weaponAccum_ = 0.f;

	const unsigned t0 = GetTickCount();
	PodSnap pod = RefreshPodSealed(wantWeapon);
	const unsigned readMs = GetTickCount() - t0;

	if (pod.stage < 0 || (pod.stage > 0 && pod.stage < 11 && !pod.valid && pod.healthStatus == 1)) {
		++faultStreak_;
		lightOkStreak_ = 0;
		if (faultStreak_ >= 8) {
			readsEnabled_ = false;
			faultCooldown_ = 15.f;
			faultStreak_ = 0;
			SFC_ERR("[GAME_READY] GameState fault streak — disabling reads for 15s (stage=%d)", pod.stage);
			Invalidate("fault_streak");
			DiagEvent("GameState.read", "fault_streak");
			return;
		}
	} else if (pod.valid) {
		faultStreak_ = 0;
		if (lightOkStreak_ < 1000)
			++lightOkStreak_;
	}

	static int cool = 0;
	if (cool-- <= 0) {
		cool = 60; // ~30s at 0.5s refresh — was spamming every second
		SFC_LOG("GameState stage=%d status=%s hp=%s:%.0f/%.0f caps=%s:%d wpn=%s:%s loc=%s warm=%d",
			pod.stage,
			pod.valid ? "VALID" : (pod.stage <= 3 ? "UNAVAILABLE" : "INVALID"),
			pod.healthStatus == 2 ? "VALID" : (pod.healthStatus == 1 ? "INVALID" : "UNAVAILABLE"),
			pod.health, pod.healthMax,
			pod.capsStatus == 2 ? "VALID" : (pod.capsStatus == 1 ? "INVALID" : "UNAVAILABLE"),
			pod.caps,
			pod.weaponStatus == 2 ? "VALID" : (pod.weaponStatus == 1 ? "INVALID" : (wantWeapon ? "SKIP" : "WARM")),
			pod.weapon[0] ? pod.weapon : (snap_.weaponName.empty() ? "-" : snap_.weaponName.c_str()),
			pod.location[0] ? pod.location : "-",
			lightOkStreak_);
	}

	if (!pod.valid) {
		snap_.valid = false;
		snap_.status = (pod.stage <= 3) ? ReadStatus::Unavailable : ReadStatus::Invalid;
		snap_.stage = pod.stage;
		DiagThrottle("GameState.read.fail", 1000, "stage=%d ms=%u", pod.stage, readMs);
		return; // keep previous HP/caps/weapon on the HUD during brief failures
	}

	PlayerSnapshot s = snap_;
	s.valid = true;
	s.status = ReadStatus::Valid;
	s.stage = pod.stage;
	s.healthStatus = static_cast<ReadStatus>(pod.healthStatus);
	s.capsStatus = static_cast<ReadStatus>(pod.capsStatus);
	s.health = pod.health;
	s.healthMax = pod.healthMax;
	s.ap = pod.ap;
	s.apMax = pod.apMax;
	s.level = pod.level;
	s.caps = pod.caps;
	if (pod.location[0]) s.location = pod.location;
	if (wantWeapon) {
		s.weaponStatus = static_cast<ReadStatus>(pod.weaponStatus);
		if (pod.weaponStatus == 2) {
			if (pod.weapon[0]) s.weaponName = pod.weapon;
			else s.weaponName = "Unarmed";
			s.ammoClip = pod.ammoClip;
			s.ammoClipMax = pod.ammoClipMax;
			s.ammoReserve = pod.ammoReserve;
		}
	}
	s.gameHour = pod.gameHour;
	s.hasPos = pod.hasPos != 0;
	s.posX = pod.posX;
	s.posY = pod.posY;
	s.posZ = pod.posZ;
	s.nearby = std::move(snap_.nearby);
	snap_ = std::move(s);
	DiagThrottle("GameState.read.ok", 5000, "stage=%d ms=%u wpn=%d warm=%d",
		pod.stage, readMs, wantWeapon ? 1 : 0, lightOkStreak_);
}

void GameState::ScanNearby(float maxDist, int maxMarkers, bool npcs, bool loot, bool doors)
{
	if (!readsEnabled_) {
		snap_.nearby.clear();
		return;
	}
	if (maxMarkers <= 0 || maxDist <= 0.f || (!npcs && !loot && !doors)) {
		snap_.nearby.clear();
		return;
	}

	void** slot = reinterpret_cast<void**>(Rel(kThePlayerAbs));
	void* player = slot ? *slot : nullptr;
	if (!player) {
		snap_.nearby.clear();
		return;
	}
	std::vector<NearbyMarker> markers;
	ScanNearbySafe(player, maxDist, maxMarkers, npcs, loot, doors, markers);
	snap_.nearby = std::move(markers);
}

} // namespace sfc
