#include "core/Feature.hpp"
#include "core/Config.hpp"
#include "core/Log.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <memory>

namespace sfc {

class SettingsFeature : public IFeature {
public:
	const char* Id() const override { return "settings"; }
	const char* Name() const override { return "Settings"; }
	FeatureCategory Category() const override { return FeatureCategory::Settings; }

	void DrawMenu() override
	{
		auto& cfg = Config::Get().Data();

		ImGui::SeparatorText("Theme");
		bool dirty = false;
		dirty |= ImGui::SliderFloat("UI Scale##set_scale", &cfg.theme.uiScale, 0.75f, 1.5f, "%.2f");
		dirty |= ImGui::SliderFloat("Opacity##set_opacity", &cfg.theme.opacity, 0.4f, 1.f, "%.2f");
		dirty |= ImGui::SliderFloat("Font Size##set_font", &cfg.theme.fontSize, 12.f, 24.f, "%.0f");
		dirty |= ImGui::SliderFloat("Anim Speed##set_anim", &cfg.theme.animSpeed, 0.25f, 2.f, "%.2f");
		dirty |= ImGui::ColorEdit3("Accent##set_accent", &cfg.theme.accentR, ImGuiColorEditFlags_Float);
		dirty |= ImGui::Checkbox("CRT Effects##set_crt", &cfg.theme.crtEffects);
		ImGui::SameLine();
		dirty |= ImGui::Checkbox("Scanlines##set_scan", &cfg.theme.scanlines);
		dirty |= ImGui::Checkbox("Glow##set_glow", &cfg.theme.glow);
		ImGui::SameLine();
		dirty |= ImGui::Checkbox("Sound FX##set_sfx", &cfg.theme.soundEffects);
		dirty |= ImGui::Checkbox("Compact Mode##set_compact", &cfg.theme.compactMode);
		ImGui::SameLine();
		dirty |= ImGui::Checkbox("Minimal Mode##set_minimal", &cfg.theme.minimalMode);

		ImGui::SeparatorText("HUD");
		dirty |= ImGui::Checkbox("HUD Enabled##set_hud", &cfg.hud.enabled);
		dirty |= ImGui::Checkbox("Live HUD (always on)##set_livehud", &cfg.hud.liveHud);
		ImGui::TextWrapped("HUD hides for ~4s after doors/loads. Turn OFF if the world goes brown again.");
		dirty |= ImGui::Checkbox("Status Bar##set_statusbar", &cfg.hud.statusBar);
		dirty |= ImGui::Checkbox("Combat Info##set_combat", &cfg.hud.combatInfo);
		dirty |= ImGui::Checkbox("World Info##set_world", &cfg.hud.worldInfo);
		dirty |= ImGui::Checkbox("FPS##set_fps", &cfg.hud.fps);
		dirty |= ImGui::Checkbox("Active Effects##set_effects", &cfg.hud.activeEffects);
		dirty |= ImGui::Checkbox("Active Preset##set_preset", &cfg.hud.activePreset);
		dirty |= ImGui::Checkbox("Notifications##set_notif", &cfg.hud.notifications);
		dirty |= ImGui::Checkbox("Objectives##set_obj", &cfg.hud.objectives);
		dirty |= ImGui::SliderFloat("HUD Scale##set_hudscale", &cfg.hud.scale, 0.5f, 2.f, "%.2f");
		dirty |= ImGui::SliderFloat("HUD Pos X##set_hudx", &cfg.hud.posX, -1.f, 1.f, "%.2f");
		dirty |= ImGui::SliderFloat("HUD Pos Y##set_hudy", &cfg.hud.posY, -1.f, 1.f, "%.2f");

		ImGui::SeparatorText("Performance");
		dirty |= ImGui::SliderInt("ESP Scan (ms)##set_espscan", &cfg.performance.espScanMs, 50, 2000);
		dirty |= ImGui::SliderInt("Max ESP Markers##set_espmax", &cfg.performance.maxEspMarkers, 32, 512);
		float espFt = cfg.performance.espMaxDistance * (6.f / 128.f);
		if (ImGui::SliderFloat("ESP Max Range (ft)##set_espdist", &espFt, 100.f, 5000.f, "%.0f")) {
			cfg.performance.espMaxDistance = espFt * (128.f / 6.f);
			dirty = true;
		}
		dirty |= ImGui::Checkbox("Prefer Event-Driven##set_events", &cfg.performance.eventDrivenPreferred);

		ImGui::SeparatorText("Controls");
		dirty |= ImGui::InputInt("Menu Toggle VK##set_menuvk", &cfg.controls.menuToggleVk);
		dirty |= ImGui::InputInt("Search Toggle VK##set_searchvk", &cfg.controls.searchToggleVk);
		dirty |= ImGui::InputInt("God Mode VK##set_godvk", &cfg.controls.godModeVk);
		dirty |= ImGui::InputInt("Full Heal VK##set_healvk", &cfg.controls.healVk);
		dirty |= ImGui::InputInt("Add Caps VK##set_capsvk", &cfg.controls.addCapsVk);
		dirty |= ImGui::Checkbox("Block Game Input When Menu Open##set_block", &cfg.controls.blockGameInputWhenMenuOpen);
		ImGui::TextDisabled("INSERT=0x2D  F1=0x70  F5=0x74  F6=0x75  F7=0x76");
		ImGui::TextDisabled("Restart / re-init needed after changing VK binds.");

		ImGui::SeparatorText("Persistence");
		dirty |= ImGui::Checkbox("Auto-load last preset##set_autoload", &cfg.autoLoadPreset);
		if (ImGui::Button("Save Config##set_save")) {
			Config::Get().MarkDirty();
			if (Config::Get().Save()) {
				Notify("Config saved");
			} else {
				Notify("Config save failed");
			}
		}
		if (dirty) Config::Get().MarkDirty();
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"settings.theme", "Theme Settings", "theme ui scale accent crt", FeatureCategory::Settings, {}});
		out.push_back({"settings.hud", "HUD Settings", "hud fps overlay", FeatureCategory::Settings, {}});
		out.push_back({"settings.controls", "Controls", "hotkey insert keybind", FeatureCategory::Settings, {}});
	}
};

std::unique_ptr<IFeature> CreateSettingsFeature()
{
	return std::make_unique<SettingsFeature>();
}

} // namespace sfc
