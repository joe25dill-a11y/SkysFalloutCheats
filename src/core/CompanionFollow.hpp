#pragma once
#include <cstdint>
#include <vector>

namespace sfc {

// Bring active teammates along after SFC teleports (coc / setpos), like Pip-Boy travel.
class CompanionFollow {
public:
	static CompanionFollow& Get();

	bool Enabled() const { return enabled_; }
	void SetEnabled(bool on) { enabled_ = on; }

	// Queue teammate snapshot + delayed moveto (snapshot runs on MainGameLoop — not EndScene).
	void RequestAfterTeleport();

	// Queues a bring for MainGameLoop when called from UI/render; returns count when run in-loop.
	int BringTeammatesNow();

	// Bring one curated companion by ref ID (original instance, not a clone).
	bool BringRef(std::uint32_t refId);

	void Tick();

	// Live scan of high/middle-high actors with PlayerTeammate (plus nearby known bases).
	// Must only be called from MainGameLoop / safe game context.
	std::vector<std::uint32_t> CollectTeammateRefs() const;

	// Safe for MainGameLoop. Used by aimbot TargetManager to skip allies.
	static bool IsPlayerTeammateActor(void* actor);
	static bool IsAllyRef(std::uint32_t refId);

private:
	bool enabled_ = true;
	bool pending_ = false;
	bool pendingSnapshot_ = false;
	bool bringNow_ = false;
	unsigned warpAtMs_ = 0;
	int stableFrames_ = 0;
	std::vector<std::uint32_t> pendingRefs_;
};

} // namespace sfc
