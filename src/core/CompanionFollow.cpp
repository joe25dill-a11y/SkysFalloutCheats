#include "core/CompanionFollow.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/Gameplay.hpp"
#include "core/GameWorkQueue.hpp"
#include "core/Diag.hpp"
#include "core/Log.hpp"
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace sfc {
namespace {

constexpr std::uintptr_t kPreferredBase = 0x00400000;
constexpr std::uintptr_t kThePlayerAbs = 0x011DEA3C;
constexpr std::uintptr_t kActorProcessMgrAbs = 0x011E0E80;
constexpr std::ptrdiff_t kOff_HighActors = 0x05C;
constexpr std::ptrdiff_t kOff_MiddleHighActors = 0x000;
constexpr std::ptrdiff_t kOff_PosX = 0x030;
constexpr std::ptrdiff_t kOff_BaseForm = 0x020;
constexpr std::ptrdiff_t kOff_FormRefID = 0x00C;
constexpr std::ptrdiff_t kOff_PlayerTeammateByte = 0x18D; // NVSE Actor: bit7 = player teammate
constexpr std::ptrdiff_t kOff_ActorFlags = 0x108;          // alt: bit 26 = teammate (FO3/FNV family)
constexpr unsigned kPostTeleportDelayMs = 5000;
constexpr int kStableOverlayFrames = 90; // ~1.5s stable after settle before moveto
constexpr float kNearbyCompanionDist = 4096.f;

constexpr std::uint32_t kKnownCompanionBases[] = {
	0x00092BD2, // Boone
	0x000E32AA, // Veronica
	0x00133FDD, // Cass
	0x0013D834, // Lily
	0x000E60EF, // Raul
	0x0010C767, // Arcade
	0x00118E71, // Rex
	0x0010C769, // ED-E
	0x001694E0,
	0x001694E2,
};

std::uintptr_t Rel(std::uintptr_t absAddr)
{
	const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
	return base + (absAddr - kPreferredBase);
}

void* PlayerPtr()
{
	__try {
		void** slot = reinterpret_cast<void**>(Rel(kThePlayerAbs));
		if (!slot) return nullptr;
		void* player = *slot;
		if (!player) return nullptr;
		const auto p = reinterpret_cast<std::uintptr_t>(player);
		if (p <= 0x10000 || p >= 0xFFF00000) return nullptr;
		return player;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

std::uint32_t FormRefId(void* form)
{
	if (!form) return 0;
	__try {
		return *reinterpret_cast<std::uint32_t*>(reinterpret_cast<char*>(form) + kOff_FormRefID);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

std::uint32_t BaseFormId(void* actor)
{
	__try {
		void* base = *reinterpret_cast<void**>(reinterpret_cast<char*>(actor) + kOff_BaseForm);
		return FormRefId(base);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

bool IsKnownCompanionBase(std::uint32_t baseId)
{
	for (auto id : kKnownCompanionBases) {
		if (id == baseId) return true;
	}
	return false;
}

bool IsPlayerTeammate(void* actor)
{
	if (!actor) return false;
	__try {
		const auto b = *reinterpret_cast<std::uint8_t*>(reinterpret_cast<char*>(actor) + kOff_PlayerTeammateByte);
		if (b & 0x80) return true;
		const auto flags = *reinterpret_cast<std::uint32_t*>(reinterpret_cast<char*>(actor) + kOff_ActorFlags);
		if (flags & 0x04000000u) return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
	return false;
}

void* LookupFormById(std::uint32_t formId)
{
	if (formId == 0) return nullptr;
	__try {
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
		constexpr std::uintptr_t kFormsMapAbs = 0x011C54C0;
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

float DistToPlayer(void* actor, void* player)
{
	__try {
		const float px = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX);
		const float py = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX + 4);
		const float pz = *reinterpret_cast<float*>(reinterpret_cast<char*>(player) + kOff_PosX + 8);
		const float x = *reinterpret_cast<float*>(reinterpret_cast<char*>(actor) + kOff_PosX);
		const float y = *reinterpret_cast<float*>(reinterpret_cast<char*>(actor) + kOff_PosX + 4);
		const float z = *reinterpret_cast<float*>(reinterpret_cast<char*>(actor) + kOff_PosX + 8);
		const float dx = x - px, dy = y - py, dz = z - pz;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 1.0e30f;
	}
}

bool BufContains(const std::uint32_t* buf, int count, std::uint32_t id)
{
	for (int i = 0; i < count; ++i) {
		if (buf[i] == id) return true;
	}
	return false;
}

void TryAddActor(void* actor, void* player, std::uint32_t* buf, int cap, int* count)
{
	if (!actor || actor == player || !buf || !count || *count >= cap) return;
	const bool teammate = IsPlayerTeammate(actor);
	const std::uint32_t baseId = BaseFormId(actor);
	const bool knownNear = IsKnownCompanionBase(baseId) && DistToPlayer(actor, player) <= kNearbyCompanionDist;
	if (!teammate && !knownNear) return;

	const std::uint32_t refId = FormRefId(actor);
	if (refId == 0 || refId == 0x14) return;
	if (BufContains(buf, *count, refId)) return;
	buf[(*count)++] = refId;
}

void WalkActorListSealed(void* listObj, void* player, std::uint32_t* buf, int cap, int* count)
{
	struct Node { void* actor; Node* next; };
	int guard = 0;
	__try {
		for (Node* n = reinterpret_cast<Node*>(listObj); n && guard < 4096; n = n->next, ++guard) {
			if (!n->actor) continue;
			TryAddActor(n->actor, player, buf, cap, count);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
}

void CollectIntoBuffer(void* player, std::uint32_t* buf, int cap, int* count)
{
	*count = 0;
	if (!player || !buf || cap <= 0) return;
	__try {
		auto* mgr = reinterpret_cast<char*>(Rel(kActorProcessMgrAbs));
		if (!mgr) return;
		WalkActorListSealed(mgr + kOff_HighActors, player, buf, cap, count);
		WalkActorListSealed(mgr + kOff_MiddleHighActors, player, buf, cap, count);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
}

int WarpRefs(const std::vector<std::uint32_t>& refs)
{
	auto& console = ConsoleBridge::Get();
	if (!console.IsReady()) return 0;
	int n = 0;
	for (auto refId : refs) {
		console.Runf("\"%08X\".moveto player", refId);
		++n;
	}
	return n;
}

} // namespace

CompanionFollow& CompanionFollow::Get()
{
	static CompanionFollow instance;
	return instance;
}

bool CompanionFollow::IsPlayerTeammateActor(void* actor)
{
	return IsPlayerTeammate(actor);
}

bool CompanionFollow::IsAllyRef(std::uint32_t refId)
{
	if (refId == 0 || refId == 0x14) return true; // player / invalid
	void* form = LookupFormById(refId);
	if (!form) return false;
	if (IsPlayerTeammate(form)) return true;
	const std::uint32_t baseId = BaseFormId(form);
	return IsKnownCompanionBase(baseId);
}

std::vector<std::uint32_t> CompanionFollow::CollectTeammateRefs() const
{
	DiagScope scope("companion.scan");
	std::vector<std::uint32_t> out;
	void* player = PlayerPtr();
	if (!player) {
		scope.Fail("no_player");
		return out;
	}

	std::uint32_t buf[64]{};
	int count = 0;
	CollectIntoBuffer(player, buf, 64, &count);
	out.assign(buf, buf + count);
	char detail[64];
	std::snprintf(detail, sizeof(detail), "count=%d", count);
	scope.Ok(detail);
	return out;
}

void CompanionFollow::RequestAfterTeleport()
{
	if (!enabled_) return;
	// Do NOT walk actor lists from EndScene/ImGui — snapshot on next MainGameLoop tick.
	pendingSnapshot_ = true;
	pending_ = true;
	pendingRefs_.clear();
	stableFrames_ = 0;
	warpAtMs_ = GetTickCount() + kPostTeleportDelayMs;
	SFC_LOG("[COMPANIONS] post-teleport follow armed (snapshot deferred to game loop)");
	DiagEvent("companion.request_after_tp", "snapshot_deferred");
}

int CompanionFollow::BringTeammatesNow()
{
	// UI button during EndScene → defer scan+moveto to MainGameLoop.
	if (GameWorkQueue::Get().ShouldDeferConsole() || GameWorkQueue::Get().InRender()) {
		bringNow_ = true;
		SFC_LOG("[COMPANIONS] Bring queued for MainGameLoop");
		DiagEvent("companion.bring_queued", nullptr);
		return 0;
	}

	DiagScope scope("companion.bring_now");
	auto refs = CollectTeammateRefs();
	const int n = WarpRefs(refs);
	char detail[64];
	std::snprintf(detail, sizeof(detail), "n=%d", n);
	if (n > 0) {
		SFC_LOG("[COMPANIONS] brought %d teammate(s)", n);
		scope.Ok(detail);
	} else {
		scope.Fail(detail);
	}
	return n;
}

bool CompanionFollow::BringRef(std::uint32_t refId)
{
	if (refId == 0) return false;
	auto& console = ConsoleBridge::Get();
	if (!console.IsReady()) return false;
	DiagEvent("companion.bring_ref", nullptr);
	console.Runf("\"%08X\".moveto player", refId);
	return true;
}

void CompanionFollow::Tick()
{
	if (bringNow_) {
		bringNow_ = false;
		BringTeammatesNow();
	}

	if (pendingSnapshot_) {
		pendingRefs_ = CollectTeammateRefs();
		pendingSnapshot_ = false;
		if (pendingRefs_.empty()) {
			SFC_LOG("[COMPANIONS] teleport: no teammates found to bring");
			pending_ = false;
			return;
		}
		SFC_LOG("[COMPANIONS] queued %zu teammate(s) to moveto after teleport", pendingRefs_.size());
	}

	if (!pending_) return;
	if (!enabled_) {
		pending_ = false;
		pendingRefs_.clear();
		stableFrames_ = 0;
		pendingSnapshot_ = false;
		return;
	}
	if (GetTickCount() < warpAtMs_) return;
	// Never moveto during loads / settle — wait for a stretch of stable frames.
	if (IsLoadingScreen() || !CanDrawOverlay()) {
		stableFrames_ = 0;
		return;
	}
	if (++stableFrames_ < kStableOverlayFrames)
		return;

	DiagScope scope("companion.moveto_post_tp");
	const int n = WarpRefs(pendingRefs_);
	char detail[64];
	std::snprintf(detail, sizeof(detail), "n=%d", n);
	scope.Ok(detail);
	SFC_LOG("[COMPANIONS] post-teleport moveto issued for %d (stable)", n);
	pending_ = false;
	pendingRefs_.clear();
	stableFrames_ = 0;
}

} // namespace sfc
