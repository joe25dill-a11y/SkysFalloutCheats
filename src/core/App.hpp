#pragma once

namespace sfc {

class App {
public:
	static App& Get();
	bool Init(const char* runtimeDir);
	void Shutdown();

	// Mutations only — NVSE MainGameLoop (console drain, companions, feature ticks).
	void TickGameWorld();

	// Safe HUD memory refresh — call from EndScene when overlay may draw (NOT MainGameLoop).
	void TickHudReads();

	// RENDER / ImGui only — cached snapshot draw.
	void OnFrame();

	bool Ready() const { return ready_; }

private:
	bool ready_ = false;
};

} // namespace sfc
