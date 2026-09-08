#include "core/Feature.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/Log.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <cstdio>
#include <memory>

namespace sfc {
namespace {

struct WeatherPreset {
	const char* name;
	const char* id;
};

const WeatherPreset kWeather[] = {
	// Verified-ish vanilla / common Mojave weathers (editor IDs).
	{"Clear", "NVWastelandClear"},
	{"Goodsprings", "NVWastelandGS"},
	{"Borders", "NVWastelandBorders"},
	{"Clear East", "NVWastelandClearEast"},
	{"Urban Clear", "NVUrbanClear"},
	{"The Strip", "NVTheStripWeather"},
	{"Overcast", "WastelandOvercast"},
	{"Dust Storm", "NVDustStorm"},
	// FormID fallbacks some installs resolve more reliably:
	{"Clear (ID)", "00064609"},
};

} // namespace

class WorldFeature : public IFeature {
public:
	const char* Id() const override { return "world"; }
	const char* Name() const override { return "World"; }
	FeatureCategory Category() const override { return FeatureCategory::World; }

	bool Init() override
	{
		if (!ConsoleBridge::Get().IsReady()) {
			available_ = false;
			unavailableReason_ = "Feature unavailable with current framework/version.";
		}
		return true;
	}

	void DrawMenu() override
	{
		if (!IsAvailable()) {
			ImGui::TextWrapped("%s", UnavailableReason());
			return;
		}

		auto& console = ConsoleBridge::Get();

		ImGui::SeparatorText("Time");
		ImGui::SliderFloat("Timescale##world_timescale", &timescale_, 0.f, 100.f, "%.1f");
		if (ImGui::Button("Apply Timescale##world_set_ts")) {
			console.Runf("set timescale to %.1f", timescale_);
			Notify("Timescale set");
		}
		ImGui::SameLine();
		if (ImGui::Button("Freeze Time (0)##world_freeze")) {
			timescale_ = 0.f;
			console.Run("set timescale to 0");
			Notify("Time frozen");
		}
		ImGui::SameLine();
		if (ImGui::Button("Normal (30)##world_normal_ts")) {
			timescale_ = 30.f;
			console.Run("set timescale to 30");
			Notify("Timescale 30");
		}

		ImGui::SliderFloat("Game Hour##world_hour", &gameHour_, 0.f, 23.99f, "%.2f");
		if (ImGui::Button("Set Game Hour##world_set_hour")) {
			console.Runf("set gamehour to %.2f", gameHour_);
			Notify("Game hour set");
		}
		ImGui::SameLine();
		if (ImGui::Button("Noon##world_noon")) {
			gameHour_ = 12.f;
			console.Run("set gamehour to 12");
		}
		ImGui::SameLine();
		if (ImGui::Button("Midnight##world_midnight")) {
			gameHour_ = 0.f;
			console.Run("set gamehour to 0");
		}
		ImGui::SameLine();
		if (ImGui::Button("Night (22)##world_night")) {
			gameHour_ = 22.f;
			console.Run("set gamehour to 22");
		}

		ImGui::SeparatorText("Weather");
		ImGui::TextDisabled("FNV has weather — dust/haze/clear. Look up at the sky after clicking.");
		for (const auto& w : kWeather) {
			ImGui::PushID(w.id);
			if (ImGui::SmallButton(w.name)) {
				console.Runf("fw %s", w.id);
				Notify(w.name);
			}
			ImGui::PopID();
			ImGui::SameLine();
		}
		ImGui::NewLine();
		ImGui::InputText("Custom weather##world_weather", weatherBuf_, sizeof(weatherBuf_));
		if (ImGui::Button("Force Weather (fw)##world_fw")) {
			console.Runf("fw %s", weatherBuf_);
			Notify("Weather forced");
		}
		ImGui::SameLine();
		if (ImGui::Button("Release Weather (rw)##world_rw")) {
			console.Run("rw");
			Notify("Weather released to normal");
		}

		ImGui::SeparatorText("World Tools");
		if (ImGui::Button("Unlock nearest door##world_unlock")) {
			console.Run("Unlock");
			Notify("Unlock sent (face a door)");
		}
		ImGui::SameLine();
		if (ImGui::Button("Activate##world_activate")) {
			console.Run("Activate");
			Notify("Activate sent");
		}
		ImGui::SameLine();
		if (ImGui::Button("Disable All Mines##world_mines")) {
			console.Run("player.disarm");
			Notify("disarm sent");
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Blood (pcb)##world_pcb")) {
			console.Run("pcb");
			Notify("pcb");
		}
		ImGui::SameLine();
		if (ImGui::Button("+1 Hour##world_plus1h")) {
			gameHour_ += 1.f;
			if (gameHour_ >= 24.f) gameHour_ -= 24.f;
			console.Runf("set gamehour to %.2f", gameHour_);
			Notify("+1 hour");
		}

		if (ImGui::Button("Kill All (ka)##world_ka")) {
			console.Run("ka");
			Notify("ka — kill all");
		}
		ImGui::SameLine();
		if (ImGui::Button("Kill Hostiles (kah)##world_kah")) {
			console.Run("player.kah");
			Notify("kah");
		}
		ImGui::SameLine();
		if (ImGui::Button("Toggle AI (tai)##world_tai")) {
			console.Run("tai");
			Notify("tai");
		}
		ImGui::SameLine();
		if (ImGui::Button("Toggle Combat AI (tcai)##world_tcai")) {
			console.Run("tcai");
			Notify("tcai");
		}

		ImGui::SeparatorText("Movement / Camera");
		if (ImGui::Button("Toggle Collision (tcl)##world_tcl")) {
			console.Run("tcl");
			tclOn_ = !tclOn_;
			Notify(tclOn_ ? "TCL on" : "TCL toggled");
		}
		ImGui::SameLine();
		if (ImGui::Button("Toggle Free Cam (tfc)##world_tfc")) {
			console.Run("tfc");
			tfcOn_ = !tfcOn_;
			Notify(tfcOn_ ? "TFC on" : "TFC toggled");
		}
		ImGui::SameLine();
		if (ImGui::Button("Toggle God Mode (tgm)##world_tgm")) {
			console.Run("tgm");
			Notify("tgm");
		}
		ImGui::TextDisabled("tcl / tfc / tgm are toggles — state may desync if used outside this menu.");
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"world.timescale", "Timescale", "time speed timescale", FeatureCategory::World, {}});
		out.push_back({"world.hour", "Game Hour", "hour time day night", FeatureCategory::World, {}});
		out.push_back({"world.weather", "Weather", "fw weather climate rain", FeatureCategory::World, {}});
		out.push_back({"world.tcl", "Toggle Collision", "tcl noclip collision", FeatureCategory::World, {}});
		out.push_back({"world.kill", "Kill All / Hostiles", "ka kah kill", FeatureCategory::World, {}});
	}

	nlohmann::json Serialize() const override
	{
		return {
			{"timescale", timescale_},
			{"gameHour", gameHour_},
			{"weather", weatherBuf_},
			{"tclOn", tclOn_},
			{"tfcOn", tfcOn_}
		};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		timescale_ = j.value("timescale", timescale_);
		gameHour_ = j.value("gameHour", gameHour_);
		tclOn_ = j.value("tclOn", tclOn_);
		tfcOn_ = j.value("tfcOn", tfcOn_);
		if (j.contains("weather") && j["weather"].is_string()) {
			std::snprintf(weatherBuf_, sizeof(weatherBuf_), "%s", j["weather"].get<std::string>().c_str());
		}
	}

private:
	float timescale_ = 30.f;
	float gameHour_ = 12.f;
	bool tclOn_ = false;
	bool tfcOn_ = false;
	char weatherBuf_[64] = "NVWastelandClear";
};

std::unique_ptr<IFeature> CreateWorldFeature()
{
	return std::make_unique<WorldFeature>();
}

} // namespace sfc
