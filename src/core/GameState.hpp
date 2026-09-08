#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace sfc {

enum class ReadStatus : std::uint8_t {
	Unavailable = 0, // subsystem off / not ready / circuit open
	Invalid = 1,     // attempted but failed / faulted
	Valid = 2
};

inline const char* ReadStatusName(ReadStatus s)
{
	switch (s) {
	case ReadStatus::Valid: return "VALID";
	case ReadStatus::Invalid: return "INVALID";
	default: return "UNAVAILABLE";
	}
}

struct NearbyMarker {
	std::string name;
	float distance = 0.f;
	std::uint8_t kind = 0; // 0 npc, 1 loot, 2 door, 3 container
	float x = 0.f, y = 0.f, z = 0.f;
	std::uint32_t refId = 0; // TESObjectREFR form id for console "xxxxxxxx".cmd
};

struct PlayerSnapshot {
	bool valid = false;
	ReadStatus status = ReadStatus::Unavailable;
	int stage = 0;

	ReadStatus healthStatus = ReadStatus::Unavailable;
	ReadStatus capsStatus = ReadStatus::Unavailable;
	ReadStatus weaponStatus = ReadStatus::Unavailable;

	float health = 0.f;
	float healthMax = 0.f;
	float ap = 0.f;
	float apMax = 0.f;
	int level = 0;
	int caps = -1; // -1 means not a real count (pair with capsStatus)
	float gameHour = -1.f;
	std::string location;

	float posX = 0.f, posY = 0.f, posZ = 0.f;
	bool hasPos = false;

	std::string weaponName;
	int ammoClip = -1;
	int ammoClipMax = -1;
	int ammoReserve = -1;

	std::vector<NearbyMarker> nearby;
};

class GameState {
public:
	static GameState& Get();
	void Tick(float dt);
	void Invalidate(const char* reason);
	void NoteExteriorStream(); // soft: pause refreshes briefly, keep last HUD
	const PlayerSnapshot& Snapshot() const { return snap_; }
	bool ReadsEnabled() const { return readsEnabled_; }

	void ScanNearby(float maxDist, int maxMarkers, bool npcs, bool loot, bool doors);

	// Merge scan requests from ESP / GRAB / etc., then flush once per interval.
	// Stops boxes flickering when two systems rewrite nearby with different filters.
	void WantNearbyScan(float maxDist, int maxMarkers, bool npcs, bool loot, bool doors);
	void FlushNearbyScan(float dt);
	void FlushNearbyScanNow(); // immediate (Grab All / Scan now)

private:
	PlayerSnapshot snap_{};
	float refreshAccum_ = 0.f;
	float weaponAccum_ = 0.f;
	float heavyAccum_ = 0.f;
	float faultCooldown_ = 0.f;
	float streamCooldown_ = 0.f; // skip AV refreshes while exterior cells stream
	int faultStreak_ = 0;
	int lightOkStreak_ = 0; // successful light reads before allowing weapon/process probes
	bool readsEnabled_ = true;

	float nearbyFlushAccum_ = 0.f;
	bool nearbyWantPending_ = false;
	float nearbyWantDist_ = 0.f;
	int nearbyWantMarkers_ = 0;
	bool nearbyWantNpcs_ = false;
	bool nearbyWantLoot_ = false;
	bool nearbyWantDoors_ = false;
};

} // namespace sfc
