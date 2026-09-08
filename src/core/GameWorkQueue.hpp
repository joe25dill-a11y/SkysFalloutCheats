#pragma once
#include <cstdint>
#include <string>

namespace sfc {

// Separates game-world mutations from D3D9 EndScene.
// Console lines enqueued during render/UI are drained on NVSE MainGameLoop
// (fallback: early EndScene if messaging unavailable).

class GameWorkQueue {
public:
	static GameWorkQueue& Get();

	void SetGameLoopAvailable(bool available);
	bool GameLoopAvailable() const { return gameLoopAvailable_; }

	void EnterGameLoop();
	void LeaveGameLoop();
	bool InGameLoop() const { return inGameLoop_; }

	void EnterRender();
	void LeaveRender();
	bool InRender() const { return inRender_; }

	bool ShouldDeferConsole() const;

	// Queue a console line. Heavy cmds (resurrect/kill/coc/…) are delayed + de-duped.
	bool EnqueueConsole(const std::string& line);
	bool EnqueueConsoleOnRef(std::uint32_t refId, const std::string& cmd);

	void DrainConsole();

	std::size_t PendingConsole() const;

private:
	static constexpr int kMaxPending = 96;
	char lines_[kMaxPending][512]{};
	std::uint32_t refIds_[kMaxPending]{};
	unsigned fireAtMs_[kMaxPending]{};
	int head_ = 0;
	int count_ = 0;
	bool gameLoopAvailable_ = false;
	bool inGameLoop_ = false;
	bool inRender_ = false;
	unsigned dropped_ = 0;
	unsigned lastHeavyRunMs_ = 0;
};

} // namespace sfc
