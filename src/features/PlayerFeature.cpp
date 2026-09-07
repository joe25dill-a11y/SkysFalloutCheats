#include "core/Feature.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/Log.hpp"
#include "core/GameState.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <memory>

namespace sfc {
namespace {

struct SpecialStat {
	const char* label;
	const char* av;
	int value;
};

} // namespace

class PlayerFeature : public IFeature {
public:
	const char* Id() const override { return "player"; }
	const char* Name() const override { return "Player"; }
	FeatureCategory Category() const override { return FeatureCategory::Player; }

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
		const auto& snap = GameState::Get().Snapshot();

		ImGui::SeparatorText("Live Status");
		if (snap.valid) {
			ImGui::Text("Level %d", snap.level);
			ImGui::Text("HP  %.0f / %.0f", snap.health, snap.healthMax);
			ImGui::Text("AP  %.0f / %.0f", snap.ap, snap.apMax);
			if (snap.caps >= 0) ImGui::Text("Caps %d", snap.caps);
			if (!snap.location.empty()) ImGui::Text("Loc  %s", snap.location.c_str());
		} else {
			ImGui::TextDisabled("Waiting for player data...");
		}

		ImGui::SeparatorText("Quick Actions");
		ImGui::TextDisabled("Hotkeys: F5 God  |  F6 Heal  |  F7 +1000 Caps");
		if (ImGui::Button("God Mode##player_quick_god")) {
			console.Run("tgm");
			godMode_ = !godMode_;
			Notify(godMode_ ? "God Mode ON" : "God Mode toggled");
		}
		ImGui::SameLine();
		if (ImGui::Button("Full Heal##player_quick_heal")) {
			if (snap.valid && snap.healthMax > 0.f)
				console.Runf("player.forceav health %.0f", snap.healthMax);
			else
				console.Run("player.forceav health 99999");
			if (snap.valid && snap.apMax > 0.f)
				console.Runf("player.forceav actionpoints %.0f", snap.apMax);
			else
				console.Run("player.forceav actionpoints 9999");
			console.Run("player.forceav radiationrads 0");
			Notify("Fully restored");
		}
		ImGui::SameLine();
		if (ImGui::Button("+1000 Caps##player_quick_caps")) {
			console.Run("player.additem 0000000f 1000");
			Notify("+1000 Caps");
		}
		ImGui::SameLine();
		if (ImGui::Button("+10k Caps##player_quick_caps10k")) {
			console.Run("player.additem 0000000f 10000");
			Notify("+10000 Caps");
		}
		ImGui::SameLine();
		if (ImGui::Button("Repair Eq (srm)##player_quick_srm")) {
			console.Run("player.srm");
			Notify("Repair attempted");
		}

		ImGui::SeparatorText("Karma / Fame");
		ImGui::TextDisabled("Vanilla console reputation helpers.");
		if (ImGui::Button("Karma +100##player_karma_up")) {
			console.Run("rewardkarma 100");
			Notify("Karma +100");
		}
		ImGui::SameLine();
		if (ImGui::Button("Karma -100##player_karma_dn")) {
			console.Run("rewardkarma -100");
			Notify("Karma -100");
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Addictions##player_addictol")) {
			console.Run("player.additem 00146C78 5");
			console.Run("player.equipitem 00146C78");
			Notify("Addictol added/equipped (use if needed)");
		}
		if (ImGui::Button("NCR Fame +##player_ncr")) {
			console.Run("player.setreputation 000F43D5 1 100");
			Notify("NCR reputation bumped (best-effort)");
		}
		ImGui::SameLine();
		if (ImGui::Button("Legion Fame +##player_legion")) {
			console.Run("player.setreputation 000F43D6 1 100");
			Notify("Legion reputation bumped (best-effort)");
		}
		ImGui::SameLine();
		if (ImGui::Button("Strip Fame +##player_strip")) {
			console.Run("player.setreputation 0011E662 1 100");
			Notify("Strip reputation bumped (best-effort)");
		}

		ImGui::SeparatorText("Health");
		ImGui::SliderInt("Heal amount##player_heal_amt", &healAmount_, 1, 9999);
		if (ImGui::Button("Full Heal##player_full_heal")) {
			if (snap.valid && snap.healthMax > 0.f)
				console.Runf("player.forceav health %.0f", snap.healthMax);
			else
				console.Run("player.forceav health 99999");
			Notify("Health restored");
		}
		ImGui::SameLine();
		if (ImGui::Button("Mod Health##player_mod_heal")) {
			console.Runf("player.modav health %d", healAmount_);
			Notify("Health modified");
		}
		ImGui::SameLine();
		if (ImGui::Button("Force Health##player_force_heal")) {
			console.Runf("player.forceav health %d", healAmount_);
			Notify("Health forced");
		}

		ImGui::SeparatorText("AP");
		ImGui::SliderInt("AP amount##player_ap_amt", &apAmount_, 1, 1000);
		if (ImGui::Button("Restore AP##player_restore_ap")) {
			if (snap.valid && snap.apMax > 0.f)
				console.Runf("player.forceav actionpoints %.0f", snap.apMax);
			else
				console.Run("player.forceav actionpoints 9999");
			Notify("Action Points restored");
		}
		ImGui::SameLine();
		if (ImGui::Button("Mod AP##player_mod_ap")) {
			console.Runf("player.modav actionpoints %d", apAmount_);
		}

		ImGui::SeparatorText("Radiation");
		ImGui::SliderInt("Rads delta##player_rad_amt", &radAmount_, -1000, 1000);
		if (ImGui::Button("Clear Rads##player_clear_rads")) {
			console.Run("player.forceav radiationrads 0");
			Notify("Radiation cleared");
		}
		ImGui::SameLine();
		if (ImGui::Button("Mod Rads##player_mod_rads")) {
			console.Runf("player.modav radiationrads %d", radAmount_);
		}

		ImGui::SeparatorText("Level / XP");
		ImGui::InputInt("Level##player_level", &level_, 1, 5);
		if (level_ < 1) level_ = 1;
		if (level_ > 100) level_ = 100;
		if (ImGui::Button("Set Level##player_set_level")) {
			console.Runf("player.setlevel %d", level_);
			Notify("Level set");
		}
		ImGui::SameLine();
		ImGui::InputInt("XP add##player_xp", &xpAmount_, 100, 1000);
		if (xpAmount_ < 0) xpAmount_ = 0;
		if (ImGui::Button("Add XP##player_add_xp")) {
			console.Runf("player.rewardxp %d", xpAmount_);
			Notify("XP rewarded");
		}

		ImGui::SeparatorText("SPECIAL");
		for (int i = 0; i < 7; ++i) {
			ImGui::PushID(i);
			ImGui::SliderInt(special_[i].label, &special_[i].value, 1, 10);
			ImGui::SameLine();
			if (ImGui::Button("Apply##player_special")) {
				console.Runf("player.forceav %s %d", special_[i].av, special_[i].value);
				Notify("SPECIAL updated");
			}
			ImGui::PopID();
		}
		if (ImGui::Button("Apply All SPECIAL##player_special_all")) {
			for (int i = 0; i < 7; ++i) {
				console.Runf("player.forceav %s %d", special_[i].av, special_[i].value);
			}
			Notify("All SPECIAL applied");
		}

		ImGui::SeparatorText("Carry Weight");
		ImGui::SliderInt("Carry weight##player_carry", &carryWeight_, 0, 9999);
		if (ImGui::Button("Force CarryWeight##player_force_carry")) {
			console.Runf("player.forceav carryweight %d", carryWeight_);
			Notify("Carry weight set");
		}
		ImGui::SameLine();
		if (ImGui::Button("Mod CarryWeight##player_mod_carry")) {
			console.Runf("player.modav carryweight %d", carryWeight_);
		}

		ImGui::SeparatorText("God Mode");
		ImGui::TextUnformatted(godMode_ ? "Status: ON" : "Status: OFF");
		if (ImGui::Checkbox("God Mode (tgm)##player_god", &godMode_)) {
			console.Run("tgm");
			Notify(godMode_ ? "God Mode enabled" : "God Mode toggled");
		}

		ImGui::SeparatorText("Restore Needs");
		ImGui::TextDisabled("Hardcore needs (hunger / thirst / sleep)");
		if (ImGui::Button("Restore All Needs##player_needs")) {
			console.Run("player.forceav hunger 0");
			console.Run("player.forceav dehydration 0");
			console.Run("player.forceav sleepdeprivation 0");
			Notify("Needs restored");
		}
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"player.god", "God Mode", "god tgm invincible", FeatureCategory::Player, {}});
		out.push_back({"player.heal", "Heal", "health heal restore hp", FeatureCategory::Player, {}});
		out.push_back({"player.rads", "Radiation", "rads radiation clear", FeatureCategory::Player, {}});
		out.push_back({"player.special", "SPECIAL", "strength perception endurance charisma intelligence agility luck", FeatureCategory::Player, {}});
	}

	nlohmann::json Serialize() const override
	{
		return {
			{"healAmount", healAmount_},
			{"apAmount", apAmount_},
			{"radAmount", radAmount_},
			{"level", level_},
			{"xpAmount", xpAmount_},
			{"carryWeight", carryWeight_},
			{"godMode", godMode_},
			{"special", {
				special_[0].value, special_[1].value, special_[2].value,
				special_[3].value, special_[4].value, special_[5].value, special_[6].value
			}}
		};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		healAmount_ = j.value("healAmount", healAmount_);
		apAmount_ = j.value("apAmount", apAmount_);
		radAmount_ = j.value("radAmount", radAmount_);
		level_ = j.value("level", level_);
		xpAmount_ = j.value("xpAmount", xpAmount_);
		carryWeight_ = j.value("carryWeight", carryWeight_);
		godMode_ = j.value("godMode", godMode_);
		if (j.contains("special") && j["special"].is_array()) {
			const auto& a = j["special"];
			for (size_t i = 0; i < a.size() && i < 7; ++i) {
				special_[i].value = a[i].get<int>();
			}
		}
	}

private:
	int healAmount_ = 100;
	int apAmount_ = 100;
	int radAmount_ = -100;
	int level_ = 1;
	int xpAmount_ = 500;
	int carryWeight_ = 300;
	bool godMode_ = false;
	SpecialStat special_[7] = {
		{"Strength##sp", "strength", 5},
		{"Perception##pe", "perception", 5},
		{"Endurance##en", "endurance", 5},
		{"Charisma##ch", "charisma", 5},
		{"Intelligence##in", "intelligence", 5},
		{"Agility##ag", "agility", 5},
		{"Luck##lk", "luck", 5},
	};
};

std::unique_ptr<IFeature> CreatePlayerFeature()
{
	return std::make_unique<PlayerFeature>();
}

} // namespace sfc
