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

struct NamedForm {
	const char* name;
	const char* formHex;
};

const NamedForm kAmmo[] = {
	{"10mm Round", "00004241"},
	{"5.56mm Round", "00004240"},
	{".308 Round", "0006B53C"},
	{"Shotgun Shell", "00028EEA"},
	{"Energy Cell", "00004485"},
	{"Microfusion Cell", "00004484"},
	{"Electron Charge Pack", "0006B53E"},
	{"Flamer Fuel", "00029363"},
	{".357 Magnum Round", "0008ED03"},
	{"5mm Round", "0006B53D"},
	{".45-70 Gov't", "00121168"},
	{".44 Magnum Round", "000CE7E2"},
	{"20 Gauge Shotgun Shell", "00121166"},
	{"Missile", "00029383"},
	{"Mini Nuke", "00020799"},
	{"40mm Grenade", "0007EA22"},
};

const NamedForm kWeapons[] = {
	{"10mm Pistol", "0000434F"},
	{"9mm Pistol", "000E3778"},
	{"Service Rifle", "000E377A"},
	{"Assault Carbine", "0008FBB4"},
	{"Cowboy Repeater", "00106FEB"},
	{"Trail Carbine", "0008ED0A"},
	{"Hunting Rifle", "00004333"},
	{"Sniper Rifle", "00004340"},
	{"Anti-Materiel Rifle", "000C40DF"},
	{"That Gun", "0011A66C"},
	{"Lucky (.357)", "00127C6B"},
	{"Maria (9mm)", "00127C6A"},
	{"Plasma Rifle", "00004331"},
	{"Laser Rifle", "00004336"},
	{"Gatling Laser", "0000432E"},
	{"Minigun", "0000433F"},
	{"Fat Man", "0000432C"},
	{"Missile Launcher", "0000432A"},
	{"Grenade Launcher", "0007EA24"},
	{"Flamer", "0000432D"},
	{"Tesla Cannon", "00103B1D"},
	{"Brush Gun", "0011A66A"},
	{"Hunting Shotgun", "000E5890"},
	{"Riot Shotgun", "0008ED0B"},
	{"Combat Knife", "00004334"},
	{"Super Sledge", "0000434C"},
	{"Thermic Lance", "0012116B"},
	{"Power Fist", "0000432B"},
};

bool NameMatches(const char* name, const char* filter)
{
	if (!filter || !filter[0]) return true;
	for (const char* h = name; *h; ++h) {
		const char* a = h;
		const char* b = filter;
		while (*a && *b) {
			char ca = *a, cb = *b;
			if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
			if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
			if (ca != cb) break;
			++a; ++b;
		}
		if (!*b) return true;
	}
	return false;
}

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

class WeaponsFeature : public IFeature {
public:
	const char* Id() const override { return "weapons"; }
	const char* Name() const override { return "Weapons"; }
	FeatureCategory Category() const override { return FeatureCategory::Weapons; }

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

		ImGui::SeparatorText("Repair");
		ImGui::SliderInt("Weapon health %%##weap_hp", &weaponHealth_, 1, 100);
		if (ImGui::Button("Repair Current (srm)##weap_srm")) {
			console.Run("player.srm");
			Notify("Repair attempted (srm)");
		}
		ImGui::SameLine();
		if (ImGui::Button("Set Weapon Health##weap_sethp")) {
			console.Runf("player.setweaponhealth %d", weaponHealth_);
			Notify("Set weapon health");
		}
		ImGui::SameLine();
		if (ImGui::Button("Repair Inventory (player.srm)##weap_srm2")) {
			console.Run("player.srm");
			Notify("srm sent");
		}

		ImGui::SeparatorText("Ammo");
		ImGui::InputInt("Ammo qty##weap_ammo_qty", &ammoQty_, 10, 100);
		if (ammoQty_ < 1) ammoQty_ = 1;
		if (ImGui::Button("Add All Ammo Types##weap_ammo_all")) {
			for (const auto& a : kAmmo)
				console.Runf("player.additem %s %d", a.formHex, ammoQty_);
			Notify("All ammo types added");
		}

		ImGui::BeginChild("##weap_ammo", ImVec2(0, 140), true);
		for (const auto& a : kAmmo) {
			ImGui::PushID(a.formHex);
			ImGui::TextUnformatted(a.name);
			ImGui::SameLine(220.f);
			if (ImGui::SmallButton("Add##weap_ammo_add")) {
				console.Runf("player.additem %s %d", a.formHex, ammoQty_);
				Notify(a.name);
			}
			ImGui::PopID();
		}
		ImGui::EndChild();

		ImGui::SeparatorText("Weapons");
		ImGui::InputInt("Weapon qty##weap_wpn_qty", &weaponQty_, 1, 5);
		if (weaponQty_ < 1) weaponQty_ = 1;
		ImGui::InputText("Filter##weap_filter", filter_, sizeof(filter_));
		ImGui::Checkbox("Also add matching ammo pack##weap_with_ammo", &withAmmo_);

		ImGui::BeginChild("##weap_list", ImVec2(0, 220), true);
		for (const auto& w : kWeapons) {
			if (!NameMatches(w.name, filter_)) continue;
			ImGui::PushID(w.formHex);
			ImGui::TextUnformatted(w.name);
			ImGui::SameLine(260.f);
			ImGui::TextDisabled("%s", w.formHex);
			ImGui::SameLine(360.f);
			if (ImGui::SmallButton("Add##weap_add")) {
				console.Runf("player.additem %s %d", w.formHex, weaponQty_);
				if (withAmmo_) {
					for (const auto& a : kAmmo)
						console.Runf("player.additem %s %d", a.formHex, ammoQty_);
				}
				Notify(w.name);
			}
			ImGui::PopID();
		}
		ImGui::EndChild();

		ImGui::SeparatorText("Custom FormID");
		ImGui::InputText("FormID (hex)##weap_form", formIdBuf_, sizeof(formIdBuf_));
		if (ImGui::Button("Add Custom##weap_custom")) {
			std::string id = SanitizeFormId(formIdBuf_);
			if (id.empty()) Notify("Invalid FormID");
			else {
				console.Runf("player.additem %s %d", id.c_str(), weaponQty_);
				Notify("Custom form added");
			}
		}
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"weapons.repair", "Repair Weapon", "repair weapon health srm condition", FeatureCategory::Weapons, {}});
		out.push_back({"weapons.ammo", "Add Ammo", "ammo bullets shells energy cell", FeatureCategory::Weapons, {}});
		out.push_back({"weapons.catalog", "Weapon Catalog", "rifle pistol unique fat man laser", FeatureCategory::Weapons, {}});
	}

	nlohmann::json Serialize() const override
	{
		return {
			{"weaponHealth", weaponHealth_},
			{"ammoQty", ammoQty_},
			{"weaponQty", weaponQty_},
			{"withAmmo", withAmmo_},
			{"formId", formIdBuf_}
		};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		weaponHealth_ = j.value("weaponHealth", weaponHealth_);
		ammoQty_ = j.value("ammoQty", ammoQty_);
		weaponQty_ = j.value("weaponQty", weaponQty_);
		withAmmo_ = j.value("withAmmo", withAmmo_);
		if (j.contains("formId") && j["formId"].is_string()) {
			std::snprintf(formIdBuf_, sizeof(formIdBuf_), "%s", j["formId"].get<std::string>().c_str());
		}
	}

private:
	int weaponHealth_ = 100;
	int ammoQty_ = 100;
	int weaponQty_ = 1;
	bool withAmmo_ = true;
	char formIdBuf_[16] = "0000434F";
	char filter_[64] = "";
};

std::unique_ptr<IFeature> CreateWeaponsFeature()
{
	return std::make_unique<WeaponsFeature>();
}

} // namespace sfc
