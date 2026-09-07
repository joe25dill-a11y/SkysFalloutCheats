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

struct CatalogItem {
	const char* name;
	const char* formHex;
	const char* category; // Aid / Chems / Armor / Misc
};

// Vanilla FNV FormIDs (base game). Quantities are chosen by the menu.
const CatalogItem kCatalog[] = {
	// Aid
	{"Stimpak", "00015169", "Aid"},
	{"Super Stimpak", "0001516A", "Aid"},
	{"Doctor's Bag", "00140A69", "Aid"},
	{"RadAway", "00015167", "Aid"},
	{"Rad-X", "00015168", "Aid"},
	{"Purified Water", "000151A3", "Aid"},
	{"Dirty Water", "000151A4", "Aid"},
	{"Healing Powder", "0013A4BB", "Aid"},
	{"Antivenom", "0013A4BA", "Aid"},
	{"Blood Pack", "00034051", "Aid"},
	// Chems
	{"Med-X", "00015166", "Chems"},
	{"Psycho", "000CD7C3", "Chems"},
	{"Buffout", "00015163", "Chems"},
	{"Mentats", "00015165", "Chems"},
	{"Jet", "00015164", "Chems"},
	{"Steady", "00146C7D", "Chems"},
	{"Rebound", "00146C7E", "Chems"},
	{"Turbo", "00146C7C", "Chems"},
	{"Slasher", "00146C7B", "Chems"},
	{"Hydra", "00146C7A", "Chems"},
	{"Party Time Mentats", "000E2C6D", "Chems"},
	{"Fixer", "00146C79", "Chems"},
	{"Addictol", "00146C78", "Chems"},
	// Armor / clothing
	{"Leather Armor", "00020423", "Armor"},
	{"Metal Armor", "0003307D", "Armor"},
	{"Combat Armor", "000CE552", "Armor"},
	{"Combat Armor, Reinforced", "001463FF", "Armor"},
	{"NCR Trooper Armor", "000E5CB3", "Armor"},
	{"NCR Ranger Patrol Armor", "001264A3", "Armor"},
	{"NCR Ranger Combat Armor", "001264A4", "Armor"},
	{"T-45d Power Armor", "000A6F76", "Armor"},
	{"T-51b Power Armor", "000A6F77", "Armor"},
	{"Remnants Power Armor", "00133168", "Armor"},
	{"Advanced Radiation Suit", "0003307F", "Armor"},
	{"Stealth Suit Mk II", "00127C6C", "Armor"},
	{"Glasses", "000CB5F6", "Armor"},
	{"Ranger Hat", "001264A0", "Armor"},
	// Misc useful
	{"Bottle Cap", "0000000F", "Misc"},
	{"Pre-War Money", "000340A4", "Misc"},
	{"Bobby Pin", "0000000A", "Misc"},
	{"Cram", "0008C55E", "Misc"},
	{"InstaMash", "0008C55C", "Misc"},
	{"Sugar Bombs", "0008C561", "Misc"},
	{"Sunset Sarsaparilla", "00103B1E", "Misc"},
	{"Scrap Metal", "00031944", "Misc"},
	{"Sensor Module", "00031945", "Misc"},
	{"Wonderglue", "00031948", "Misc"},
	{"Duct Tape", "0003193F", "Misc"},
	{"Empty Syringe", "0001516B", "Misc"},
	{"Surgical Tubing", "00031947", "Misc"},
	{"Fission Battery", "00031946", "Misc"},
	{"Conductor", "0003193E", "Misc"},
};

bool PassesFilter(const CatalogItem& it, const char* filter, const char* catFilter)
{
	if (catFilter && catFilter[0] && std::strcmp(it.category, catFilter) != 0) return false;
	if (!filter || !filter[0]) return true;
	// case-insensitive substring on name
	const char* hay = it.name;
	for (const char* h = hay; *h; ++h) {
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

class InventoryFeature : public IFeature {
public:
	const char* Id() const override { return "inventory"; }
	const char* Name() const override { return "Inventory"; }
	FeatureCategory Category() const override { return FeatureCategory::Inventory; }

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

		ImGui::SeparatorText("One-Click Kits");
		if (ImGui::Button("Survivor Kit##inv_kit_surv")) {
			console.Run("player.additem 00015169 50");
			console.Run("player.additem 0001516A 15");
			console.Run("player.additem 00015167 20");
			console.Run("player.additem 00015168 10");
			console.Run("player.additem 0000000A 50");
			console.Run("player.additem 0000000F 5000");
			console.Run("player.additem 00020423 1");
			console.Run("player.additem 0000434F 1");
			console.Run("player.additem 00004241 200");
			Notify("Survivor kit added");
		}
		ImGui::SameLine();
		if (ImGui::Button("Gunslinger Kit##inv_kit_gun")) {
			console.Run("player.additem 00106FEB 1");
			console.Run("player.additem 0008ED03 200");
			console.Run("player.additem 000E377A 1");
			console.Run("player.additem 00004240 200");
			console.Run("player.additem 0000434F 1");
			console.Run("player.additem 00004241 200");
			console.Run("player.additem 001264A4 1");
			Notify("Gunslinger kit added");
		}
		ImGui::SameLine();
		if (ImGui::Button("Repair Junk##inv_kit_junk")) {
			console.Run("player.additem 00031944 25");
			console.Run("player.additem 0003193F 25");
			console.Run("player.additem 00031948 25");
			console.Run("player.additem 00031945 15");
			console.Run("player.additem 00031946 15");
			console.Run("player.additem 0003193E 15");
			Notify("Repair components added");
		}

		ImGui::SeparatorText("Catalog");
		ImGui::InputInt("Quantity##inv_qty", &quantity_, 1, 10);
		if (quantity_ < 1) quantity_ = 1;
		ImGui::InputText("Filter##inv_filter", filter_, sizeof(filter_));
		ImGui::Combo("Category##inv_cat", &catIdx_, "All\0Aid\0Chems\0Armor\0Misc\0");

		const char* catFilter = nullptr;
		switch (catIdx_) {
		case 1: catFilter = "Aid"; break;
		case 2: catFilter = "Chems"; break;
		case 3: catFilter = "Armor"; break;
		case 4: catFilter = "Misc"; break;
		default: break;
		}

		if (ImGui::Button("Add All Visible##inv_add_visible")) {
			int n = 0;
			for (const auto& it : kCatalog) {
				if (!PassesFilter(it, filter_, catFilter)) continue;
				console.Runf("player.additem %s %d", it.formHex, quantity_);
				++n;
			}
			Notify(n > 0 ? "Added visible catalog items" : "Nothing matched filter");
		}
		ImGui::SameLine();
		if (ImGui::Button("Aid Pack##inv_aid_pack")) {
			console.Runf("player.additem 00015169 %d", quantity_ * 25);
			console.Runf("player.additem 0001516A %d", quantity_ * 10);
			console.Runf("player.additem 00015167 %d", quantity_ * 10);
			console.Runf("player.additem 00015168 %d", quantity_ * 5);
			console.Runf("player.additem 00140A69 %d", quantity_ * 5);
			Notify("Aid pack added");
		}
		ImGui::SameLine();
		if (ImGui::Button("Chem Pack##inv_chem_pack")) {
			console.Runf("player.additem 00015166 %d", quantity_ * 10);
			console.Runf("player.additem 000CD7C3 %d", quantity_ * 10);
			console.Runf("player.additem 00015163 %d", quantity_ * 10);
			console.Runf("player.additem 00015165 %d", quantity_ * 10);
			console.Runf("player.additem 00015164 %d", quantity_ * 10);
			Notify("Chem pack added");
		}

		ImGui::BeginChild("##inv_catalog", ImVec2(0, 280), true);
		for (const auto& it : kCatalog) {
			if (!PassesFilter(it, filter_, catFilter)) continue;
			ImGui::PushID(it.formHex);
			ImGui::Text("[%s] %s", it.category, it.name);
			ImGui::SameLine(280.f);
			ImGui::TextDisabled("%s", it.formHex);
			ImGui::SameLine(380.f);
			if (ImGui::SmallButton("Add##inv_cat_add")) {
				console.Runf("player.additem %s %d", it.formHex, quantity_);
				Notify(it.name);
			}
			ImGui::PopID();
		}
		ImGui::EndChild();

		ImGui::SeparatorText("Custom FormID");
		ImGui::InputText("FormID (hex)##inv_form", formIdBuf_, sizeof(formIdBuf_));
		if (ImGui::Button("Add Custom##inv_add")) {
			std::string id = SanitizeFormId(formIdBuf_);
			if (id.empty()) Notify("Invalid FormID");
			else {
				console.Runf("player.additem %s %d", id.c_str(), quantity_);
				Notify("Item added");
			}
		}

		ImGui::SeparatorText("Weight / Clean");
		ImGui::TextWrapped("Raise carry weight from Player, or use God Mode. Remove-all is irreversible.");
		ImGui::Checkbox("I understand removeall is irreversible##inv_confirm", &confirmRemoveAll_);
		ImGui::BeginDisabled(!confirmRemoveAll_);
		if (ImGui::Button("Remove All Items##inv_removeall")) {
			console.Run("player.removeallitems");
			Notify("All items removed");
			confirmRemoveAll_ = false;
		}
		ImGui::EndDisabled();
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"inventory.catalog", "Item Catalog", "stimpak aid chem armor additem", FeatureCategory::Inventory, {}});
		out.push_back({"inventory.add", "Add Item", "additem formid inventory spawn", FeatureCategory::Inventory, {}});
		out.push_back({"inventory.removeall", "Remove All Items", "removeall clear inventory danger", FeatureCategory::Inventory, {}});
	}

	nlohmann::json Serialize() const override
	{
		return {{"quantity", quantity_}, {"formId", formIdBuf_}, {"catIdx", catIdx_}};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		quantity_ = j.value("quantity", quantity_);
		catIdx_ = j.value("catIdx", catIdx_);
		if (j.contains("formId") && j["formId"].is_string()) {
			std::snprintf(formIdBuf_, sizeof(formIdBuf_), "%s", j["formId"].get<std::string>().c_str());
		}
	}

private:
	int quantity_ = 1;
	int catIdx_ = 0;
	bool confirmRemoveAll_ = false;
	char formIdBuf_[16] = "00015169";
	char filter_[64] = "";
};

std::unique_ptr<IFeature> CreateInventoryFeature()
{
	return std::make_unique<InventoryFeature>();
}

} // namespace sfc
