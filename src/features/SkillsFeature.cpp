#include "core/Feature.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/Log.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace sfc {
namespace {

struct SkillRow {
	const char* label;
	const char* av;
	int value;
};

struct PerkRow {
	const char* name;
	const char* formHex;
};

const PerkRow kPerks[] = {
	{"Educated", "00031DBC"},
	{"Comprehension", "00031DE1"},
	{"Intense Training", "00044AF0"},
	{"Toughness", "00031DB1"},
	{"Life Giver", "00031DB2"},
	{"Action Boy", "00031DB3"},
	{"Strong Back", "00031DDE"},
	{"Swift Learner", "00031DDC"},
	{"Rapid Reload", "00031DBA"},
	{"Gunslinger", "00031DB9"},
	{"Commando", "00031DB8"},
	{"Sniper", "00031DB7"},
	{"Better Criticals", "00031DB6"},
	{"Nerves of Steel", "00031DB5"},
	{"Silent Running", "00031DDF"},
	{"Ninja", "00031DE0"},
	{"Robotics Expert", "00031DDD"},
	{"Science", "00031DDA"},
	{"Tag!", "00031DDB"},
	{"Jury Rigging", "00165816"},
	{"Math Wrath", "00135F18"},
	{"Confirmed Bachelor", "00135F19"},
	{"Cherchez La Femme", "00135F1A"},
	{"Travel Light", "00135F1B"},
	{"Hunter", "00135F1C"},
	{"Entomologist", "00135F1D"},
};

std::string SanitizeFormId(const char* raw)
{
	if (!raw) return {};
	std::string out;
	for (const char* p = raw; *p; ++p) {
		const char c = *p;
		if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) out.push_back(c);
	}
	if (out.empty() || out.size() > 8) return {};
	while (out.size() < 8) out.insert(out.begin(), '0');
	return out;
}

} // namespace

class SkillsFeature : public IFeature {
public:
	const char* Id() const override { return "skills"; }
	const char* Name() const override { return "Skills"; }
	FeatureCategory Category() const override { return FeatureCategory::Skills; }

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

		ImGui::SeparatorText("Skills");
		ImGui::SliderInt("Bulk value##skills_bulk", &bulkValue_, 0, 100);
		if (ImGui::Button("Set All Skills##skills_set_all")) {
			for (auto& s : skills_)
				s.value = bulkValue_;
			ApplyAll(console);
			Notify("All skills set");
		}
		ImGui::SameLine();
		if (ImGui::Button("Max All (100)##skills_max")) {
			for (auto& s : skills_) s.value = 100;
			ApplyAll(console);
			Notify("All skills maxed");
		}
		ImGui::SameLine();
		if (ImGui::Button("Apply Listed##skills_apply")) {
			ApplyAll(console);
			Notify("Skills applied");
		}

		ImGui::BeginChild("##skills_list", ImVec2(0, 260), true);
		for (int i = 0; i < 13; ++i) {
			ImGui::PushID(i);
			ImGui::SliderInt(skills_[i].label, &skills_[i].value, 0, 100);
			ImGui::SameLine();
			if (ImGui::SmallButton("Set##skills_one")) {
				console.Runf("player.forceav %s %d", skills_[i].av, skills_[i].value);
				Notify(skills_[i].label);
			}
			ImGui::PopID();
		}
		ImGui::EndChild();

		ImGui::SeparatorText("Perks");
		ImGui::TextDisabled("addperk uses base FormIDs. Some perks need ranks / DLC.");
		ImGui::InputText("Filter##perk_filter", perkFilter_, sizeof(perkFilter_));
		ImGui::BeginChild("##perk_list", ImVec2(0, 200), true);
		for (const auto& p : kPerks) {
			if (perkFilter_[0]) {
				bool ok = false;
				for (const char* h = p.name; *h; ++h) {
					const char* a = h;
					const char* b = perkFilter_;
					while (*a && *b) {
						char ca = *a, cb = *b;
						if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
						if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
						if (ca != cb) break;
						++a; ++b;
					}
					if (!*b) { ok = true; break; }
				}
				if (!ok) continue;
			}
			ImGui::PushID(p.formHex);
			ImGui::TextUnformatted(p.name);
			ImGui::SameLine(260.f);
			ImGui::TextDisabled("%s", p.formHex);
			ImGui::SameLine(360.f);
			if (ImGui::SmallButton("Add##perk_add")) {
				console.Runf("player.addperk %s", p.formHex);
				Notify(p.name);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Remove##perk_rem")) {
				console.Runf("player.removeperk %s", p.formHex);
				Notify("Perk removed");
			}
			ImGui::PopID();
		}
		ImGui::EndChild();

		ImGui::SeparatorText("Custom Perk FormID");
		ImGui::InputText("Perk FormID##perk_custom", perkForm_, sizeof(perkForm_));
		if (ImGui::Button("Add Custom Perk##perk_custom_add")) {
			std::string id = SanitizeFormId(perkForm_);
			if (id.empty()) Notify("Invalid FormID");
			else {
				console.Runf("player.addperk %s", id.c_str());
				Notify("Perk added");
			}
		}
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"skills.max", "Max Skills", "skills guns sneak speech 100", FeatureCategory::Skills, {}});
		out.push_back({"skills.perks", "Perks", "addperk perk toughness educated", FeatureCategory::Perks, {}});
	}

	nlohmann::json Serialize() const override
	{
		nlohmann::json arr = nlohmann::json::array();
		for (int i = 0; i < 13; ++i) arr.push_back(skills_[i].value);
		return {{"bulk", bulkValue_}, {"skills", arr}, {"perkForm", perkForm_}};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		bulkValue_ = j.value("bulk", bulkValue_);
		if (j.contains("skills") && j["skills"].is_array()) {
			const auto& a = j["skills"];
			for (size_t i = 0; i < a.size() && i < 13; ++i)
				skills_[i].value = a[i].get<int>();
		}
		if (j.contains("perkForm") && j["perkForm"].is_string()) {
			std::snprintf(perkForm_, sizeof(perkForm_), "%s", j["perkForm"].get<std::string>().c_str());
		}
	}

private:
	void ApplyAll(ConsoleBridge& console)
	{
		for (auto& s : skills_)
			console.Runf("player.forceav %s %d", s.av, s.value);
	}

	int bulkValue_ = 100;
	char perkFilter_[64] = "";
	char perkForm_[16] = "00031DBC";
	SkillRow skills_[13] = {
		{"Guns##sk", "guns", 50},
		{"Energy Weapons##sk", "energyweapons", 50},
		{"Explosives##sk", "explosives", 50},
		{"Melee Weapons##sk", "meleeweapons", 50},
		{"Unarmed##sk", "unarmed", 50},
		{"Medicine##sk", "medicine", 50},
		{"Lockpick##sk", "lockpick", 50},
		{"Science##sk", "science", 50},
		{"Repair##sk", "repair", 50},
		{"Sneak##sk", "sneak", 50},
		{"Speech##sk", "speech", 50},
		{"Barter##sk", "barter", 50},
		{"Survival##sk", "survival", 50},
	};
};

std::unique_ptr<IFeature> CreateSkillsFeature()
{
	return std::make_unique<SkillsFeature>();
}

} // namespace sfc
