#include "core/Config.hpp"
#include "core/Log.hpp"
#include <fstream>

namespace sfc {

Config& Config::Get()
{
	static Config instance;
	return instance;
}

nlohmann::json ToJson(const AppConfig& c)
{
	return nlohmann::json{
		{"schemaVersion", c.schemaVersion},
		{"lastPreset", c.lastPreset},
		{"autoLoadPreset", c.autoLoadPreset},
		{"theme", {
			{"uiScale", c.theme.uiScale},
			{"opacity", c.theme.opacity},
			{"accent", {c.theme.accentR, c.theme.accentG, c.theme.accentB}},
			{"fontSize", c.theme.fontSize},
			{"animSpeed", c.theme.animSpeed},
			{"crtEffects", c.theme.crtEffects},
			{"scanlines", c.theme.scanlines},
			{"glow", c.theme.glow},
			{"soundEffects", c.theme.soundEffects},
			{"compactMode", c.theme.compactMode},
			{"minimalMode", c.theme.minimalMode}
		}},
		{"hud", {
			{"enabled", c.hud.enabled},
			{"liveHud", c.hud.liveHud},
			{"statusBar", c.hud.statusBar},
			{"combatInfo", c.hud.combatInfo},
			{"worldInfo", c.hud.worldInfo},
			{"fps", c.hud.fps},
			{"activeEffects", c.hud.activeEffects},
			{"activePreset", c.hud.activePreset},
			{"notifications", c.hud.notifications},
			{"objectives", c.hud.objectives},
			{"scale", c.hud.scale},
			{"posX", c.hud.posX},
			{"posY", c.hud.posY}
		}},
		{"performance", {
			{"espScanMs", c.performance.espScanMs},
			{"maxEspMarkers", c.performance.maxEspMarkers},
			{"espMaxDistance", c.performance.espMaxDistance},
			{"eventDrivenPreferred", c.performance.eventDrivenPreferred},
			{"espEnabled", c.performance.espEnabled}
		}},
		{"diagnostics", {
			{"enabled", c.diagnostics.enabled},
			{"isolationMode", c.diagnostics.isolationMode}
		}},
		{"controls", {
			{"menuToggleVk", c.controls.menuToggleVk},
			{"searchToggleVk", c.controls.searchToggleVk},
			{"godModeVk", c.controls.godModeVk},
			{"healVk", c.controls.healVk},
			{"addCapsVk", c.controls.addCapsVk},
			{"blockGameInputWhenMenuOpen", c.controls.blockGameInputWhenMenuOpen}
		}}
	};
}

void FromJson(const nlohmann::json& j, AppConfig& c)
{
	if (!j.is_object()) return;
	c.schemaVersion = j.value("schemaVersion", c.schemaVersion);
	c.lastPreset = j.value("lastPreset", c.lastPreset);
	c.autoLoadPreset = j.value("autoLoadPreset", c.autoLoadPreset);
	if (j.contains("theme")) {
		auto& t = j["theme"];
		c.theme.uiScale = t.value("uiScale", c.theme.uiScale);
		c.theme.opacity = t.value("opacity", c.theme.opacity);
		if (t.contains("accent") && t["accent"].is_array() && t["accent"].size() >= 3) {
			c.theme.accentR = t["accent"][0];
			c.theme.accentG = t["accent"][1];
			c.theme.accentB = t["accent"][2];
		}
		c.theme.fontSize = t.value("fontSize", c.theme.fontSize);
		c.theme.animSpeed = t.value("animSpeed", c.theme.animSpeed);
		c.theme.crtEffects = t.value("crtEffects", c.theme.crtEffects);
		c.theme.scanlines = t.value("scanlines", c.theme.scanlines);
		c.theme.glow = t.value("glow", c.theme.glow);
		c.theme.soundEffects = t.value("soundEffects", c.theme.soundEffects);
		c.theme.compactMode = t.value("compactMode", c.theme.compactMode);
		c.theme.minimalMode = t.value("minimalMode", c.theme.minimalMode);
	}
	if (j.contains("hud")) {
		auto& h = j["hud"];
		c.hud.enabled = h.value("enabled", c.hud.enabled);
		c.hud.liveHud = h.value("liveHud", c.hud.liveHud);
		c.hud.statusBar = h.value("statusBar", c.hud.statusBar);
		c.hud.combatInfo = h.value("combatInfo", c.hud.combatInfo);
		c.hud.worldInfo = h.value("worldInfo", c.hud.worldInfo);
		c.hud.fps = h.value("fps", c.hud.fps);
		c.hud.activeEffects = h.value("activeEffects", c.hud.activeEffects);
		c.hud.activePreset = h.value("activePreset", c.hud.activePreset);
		c.hud.notifications = h.value("notifications", c.hud.notifications);
		c.hud.objectives = h.value("objectives", c.hud.objectives);
		c.hud.scale = h.value("scale", c.hud.scale);
		c.hud.posX = h.value("posX", c.hud.posX);
		c.hud.posY = h.value("posY", c.hud.posY);
	}
	if (j.contains("performance")) {
		auto& p = j["performance"];
		c.performance.espScanMs = p.value("espScanMs", c.performance.espScanMs);
		c.performance.maxEspMarkers = p.value("maxEspMarkers", c.performance.maxEspMarkers);
		c.performance.espMaxDistance = p.value("espMaxDistance", c.performance.espMaxDistance);
		c.performance.eventDrivenPreferred = p.value("eventDrivenPreferred", c.performance.eventDrivenPreferred);
		c.performance.espEnabled = p.value("espEnabled", c.performance.espEnabled);
	}
	if (j.contains("diagnostics")) {
		auto& d = j["diagnostics"];
		c.diagnostics.enabled = d.value("enabled", c.diagnostics.enabled);
		c.diagnostics.isolationMode = d.value("isolationMode", c.diagnostics.isolationMode);
	}
	if (j.contains("controls")) {
		auto& k = j["controls"];
		c.controls.menuToggleVk = k.value("menuToggleVk", c.controls.menuToggleVk);
		c.controls.searchToggleVk = k.value("searchToggleVk", c.controls.searchToggleVk);
		c.controls.godModeVk = k.value("godModeVk", c.controls.godModeVk);
		c.controls.healVk = k.value("healVk", c.controls.healVk);
		c.controls.addCapsVk = k.value("addCapsVk", c.controls.addCapsVk);
		c.controls.blockGameInputWhenMenuOpen = k.value("blockGameInputWhenMenuOpen", c.controls.blockGameInputWhenMenuOpen);
	}
}

bool Config::Load(const std::filesystem::path& path)
{
	path_ = path;
	std::ifstream in(path);
	if (!in) {
		SFC_WARN("Config not found, using defaults: %s", path.string().c_str());
		dirty_ = true;
		return false;
	}
	try {
		nlohmann::json j;
		in >> j;
		FromJson(j, data_);
		dirty_ = false;
		SFC_LOG("Config loaded: %s", path.string().c_str());
		return true;
	} catch (const std::exception& e) {
		SFC_ERR("Config parse failed: %s", e.what());
		return false;
	}
}

bool Config::Save() const
{
	if (path_.empty()) return false;
	try {
		std::filesystem::create_directories(path_.parent_path());
		auto tmp = path_;
		tmp += ".tmp";
		{
			std::ofstream out(tmp);
			out << ToJson(data_).dump(2);
		}
		std::filesystem::rename(tmp, path_);
		dirty_ = false;
		SFC_LOG("Config saved");
		return true;
	} catch (const std::exception& e) {
		SFC_ERR("Config save failed: %s", e.what());
		return false;
	}
}

void Config::SaveIfDirty()
{
	if (dirty_) Save();
}

} // namespace sfc
