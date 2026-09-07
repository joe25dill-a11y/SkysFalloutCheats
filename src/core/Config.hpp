#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <filesystem>

namespace sfc {

struct ThemeSettings {
	float uiScale = 1.0f;
	float opacity = 0.92f;
	float accentR = 1.0f, accentG = 0.72f, accentB = 0.20f;
	float fontSize = 16.0f;
	float animSpeed = 1.0f;
	bool crtEffects = false;
	bool scanlines = false;
	bool glow = false;
	bool soundEffects = true;
	bool compactMode = false;
	bool minimalMode = false;
};

struct HudSettings {
	bool enabled = true;
	bool liveHud = true;
	bool statusBar = true;
	bool combatInfo = true;
	bool worldInfo = true;
	bool fps = false;
	bool activeEffects = true;
	bool activePreset = false;
	bool notifications = true;
	bool objectives = true;
	float scale = 1.0f;
	float posX = 0.0f;
	float posY = 0.0f;
};

struct PerformanceSettings {
	int espScanMs = 250;
	int maxEspMarkers = 128;
	float espMaxDistance = 8000.0f;
	bool eventDrivenPreferred = true;
	bool espEnabled = false;
};

struct DiagnosticsSettings {
	// Forensic [DIAG] seq logs for crash/freeze investigation (throttled).
	bool enabled = true;
	// Isolation test mode — see Isolation.hpp. "off" = full product.
	std::string isolationMode = "off";
};

struct ControlSettings {
	int menuToggleVk = 0x2D; // VK_INSERT
	int searchToggleVk = 0x70; // VK_F1
	int godModeVk = 0x74; // VK_F5
	int healVk = 0x75; // VK_F6
	int addCapsVk = 0x76; // VK_F7
	bool blockGameInputWhenMenuOpen = true;
};

struct AppConfig {
	int schemaVersion = 1;
	ThemeSettings theme;
	HudSettings hud;
	PerformanceSettings performance;
	ControlSettings controls;
	DiagnosticsSettings diagnostics;
	std::string lastPreset;
	bool autoLoadPreset = false;
};

class Config {
public:
	static Config& Get();

	bool Load(const std::filesystem::path& path);
	bool Save() const;
	void SetPath(const std::filesystem::path& path) { path_ = path; }
	const std::filesystem::path& Path() const { return path_; }

	AppConfig& Data() { return data_; }
	const AppConfig& Data() const { return data_; }

	void MarkDirty() { dirty_ = true; }
	bool IsDirty() const { return dirty_; }
	void SaveIfDirty();

private:
	AppConfig data_{};
	std::filesystem::path path_;
	mutable bool dirty_ = false;
};

nlohmann::json ToJson(const AppConfig& c);
void FromJson(const nlohmann::json& j, AppConfig& c);

} // namespace sfc
