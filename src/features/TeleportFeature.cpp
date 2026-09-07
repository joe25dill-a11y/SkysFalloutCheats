#include "core/Feature.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/Compat.hpp"
#include "core/CompanionFollow.hpp"
#include "core/Gameplay.hpp"
#include "core/GameState.hpp"
#include "core/Input.hpp"
#include "core/Log.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sfc {
namespace {

struct LocationEntry {
	std::string name;
	float x = 0.f, y = 0.f, z = 0.f;
	bool favorite = false;
};

std::filesystem::path LocationsPath()
{
	return std::filesystem::path(Compat().pluginDataDir) / "locations.json";
}

bool PlaceLabelMatch(const char* label, const char* filter)
{
	if (!filter || !filter[0]) return true;
	for (const char* h = label; *h; ++h) {
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

} // namespace

struct PlaceEntry {
	const char* label;
	const char* cell;
};

class TeleportFeature : public IFeature {
public:
	const char* Id() const override { return "teleport"; }
	const char* Name() const override { return "Teleport"; }
	FeatureCategory Category() const override { return FeatureCategory::Teleport; }

	bool Init() override
	{
		if (!ConsoleBridge::Get().IsReady()) {
			available_ = false;
			unavailableReason_ = "Feature unavailable with current framework/version.";
		}
		LoadLocations();
		return true;
	}

	void Tick(float /*dt*/) override
	{
		if (!pending_.active) return;
		if (GetTickCount() < pending_.fireAtMs) return;
		auto& console = ConsoleBridge::Get();
		if (!console.IsReady()) {
			pending_.active = false;
			return;
		}
		// Don't fire mid-loading — wait until the load screen drops.
		if (IsLoadingScreen()) {
			pending_.fireAtMs = GetTickCount() + 100;
			return;
		}
		for (int i = 0; i < pending_.lineCount; ++i) {
			console.Run(pending_.lines[i]);
		}
		SFC_LOG("[TELEPORT] fired (%d cmd)", pending_.lineCount);
		pending_.active = false;
		pending_.lineCount = 0;
	}

	void DrawMenu() override
	{
		if (!IsAvailable()) {
			ImGui::TextWrapped("%s", UnavailableReason());
			return;
		}

		const auto& snap = GameState::Get().Snapshot();

		ImGui::SeparatorText("Companions");
		{
		ImGui::TextDisabled("Teammates moveto you after warps (waits for stable world).");
			if (ImGui::Button("Bring companions here now##tp_bring")) {
				const int n = CompanionFollow::Get().BringTeammatesNow();
				if (n > 0) Notify("Companions moving to you");
				else Notify("No active teammates found nearby");
			}
			bool follow = CompanionFollow::Get().Enabled();
			if (ImGui::Checkbox("Companions follow teleports##tp_comp_follow", &follow)) {
				CompanionFollow::Get().SetEnabled(follow);
			}
		}

		ImGui::SeparatorText("Quick Places");
		ImGui::TextWrapped(
			"Town jumps use coc (same as console). Menu closes for one frame, then warps — "
			"you do not need to discover the place or finish Goodsprings first.");
		if (pending_.active) {
			ImGui::TextColored(ImVec4(1.f, 0.85f, 0.3f, 1.f), "Warp queued…");
		}

		// Verified FalloutNV.esm editor IDs (wiki cell list). Avoid dummy/bugged cells.
		static const PlaceEntry kExteriors[] = {
			{"Goodsprings", "Goodsprings"},
			{"Primm", "Primm"},
			{"Nipton", "Nipton"},
			{"Novac", "Novac"},
			{"Boulder City", "BoulderCity"},
			{"Hidden Valley", "HiddenValley"},
			{"Freeside North", "FreesideNorthGate"},
			{"Freeside East", "FreesideEastGate"},
			{"Strip (Lucky 38)", "StripLucky38"},
			{"Jacobstown", "Jacobstown"},
			{"Nellis AFB", "NellisFrontGate"},
			{"Mojave Outpost", "MojaveOutpost"},
			{"Helios One", "HeliosOne"},
			{"McCarran Gate", "CampMcCarranGate"},
		};
		static const PlaceEntry kInteriors[] = {
			{"Doc Mitchell", "GSDocMitchellHouse"},
			{"Old Mormon Fort", "FreesideFortInterior"},
			{"Lucky 38 Suite", "Lucky38SuiteFloor22"},
			{"Lucky 38 Penthouse", "Lucky38Penthouse"},
			{"Gomorrah", "Gomorrah01"},
			{"Tops Casino", "TOPSCasino"},
			{"Ultra-Luxe", "ULCasino"},
			{"McCarran Concourse", "CampMCTermInt01"},
			{"Hoover Dam Deck", "HooverDamIntODeck"},
			{"Hidden Valley L1", "HiddenValley01"},
			{"Vault 21", "2EOVault21"},
		};

		ImGui::InputText("Filter places##tp_place_filter", placeFilter_, sizeof(placeFilter_));
		ImGui::TextUnformatted("Towns / exteriors");
		DrawPlaceGrid("##tp_ext", kExteriors, static_cast<int>(sizeof(kExteriors) / sizeof(kExteriors[0])));
		ImGui::TextUnformatted("Interiors");
		DrawPlaceGrid("##tp_int", kInteriors, static_cast<int>(sizeof(kInteriors) / sizeof(kInteriors[0])));

		if (ImGui::Button("Moveto Quest Target##tp_movetoqt")) {
			QueueCocOrCmd("player.movetoqt", "movetoqt");
		}
		ImGui::SameLine();
		if (ImGui::Button("Reveal Map (tmm 1)##tp_tmm")) {
			ConsoleBridge::Get().Run("tmm 1");
			Notify("All map markers revealed");
		}

		ImGui::SeparatorText("Live Position");
		if (snap.hasPos) {
			ImGui::Text("Current: %.2f, %.2f, %.2f", snap.posX, snap.posY, snap.posZ);
			if (ImGui::Button("Copy Live XYZ##tp_copy")) {
				posX_ = snap.posX;
				posY_ = snap.posY;
				posZ_ = snap.posZ;
				Notify("Copied live position");
			}
			ImGui::SameLine();
			if (ImGui::Button("Save Live As Location##tp_save_live")) {
				if (nameBuf_[0] == '\0') {
					Notify("Enter a location name first");
				} else {
					LocationEntry e;
					e.name = nameBuf_;
					e.x = snap.posX;
					e.y = snap.posY;
					e.z = snap.posZ;
					locations_.push_back(std::move(e));
					SaveLocations();
					Notify("Live location saved");
				}
			}
		} else {
			ImGui::TextDisabled("Position unavailable until in-world.");
		}

		ImGui::SeparatorText("Manual Position (setpos)");
		ImGui::TextDisabled("Same worldspace only — best for fine moves / saved slots.");
		ImGui::InputFloat("X##tp_x", &posX_, 10.f, 100.f, "%.2f");
		ImGui::InputFloat("Y##tp_y", &posY_, 10.f, 100.f, "%.2f");
		ImGui::InputFloat("Z##tp_z", &posZ_, 10.f, 100.f, "%.2f");
		if (ImGui::Button("Teleport (setpos)##tp_setpos")) {
			QueueSetpos(posX_, posY_, posZ_, true);
			Notify("Queued setpos");
		}

		ImGui::SeparatorText("Moveto Reference");
		ImGui::InputText("Ref FormID##tp_moveto", movetoBuf_, sizeof(movetoBuf_));
		if (ImGui::Button("player.moveto##tp_do_moveto")) {
			char line[64];
			std::snprintf(line, sizeof(line), "player.moveto %s", movetoBuf_);
			QueueCocOrCmd(line, "moveto");
		}

		ImGui::SeparatorText("Named Locations");
		ImGui::TextWrapped("Use Copy/Save Live above while standing somewhere useful, then Go from the list.");
		ImGui::InputText("Name##tp_name", nameBuf_, sizeof(nameBuf_));
		if (ImGui::Button("Save Manual XYZ As Location##tp_save")) {
			if (nameBuf_[0] == '\0') {
				Notify("Enter a location name");
			} else {
				LocationEntry e;
				e.name = nameBuf_;
				e.x = posX_;
				e.y = posY_;
				e.z = posZ_;
				locations_.push_back(std::move(e));
				SaveLocations();
				Notify("Location saved");
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload JSON##tp_reload")) {
			LoadLocations();
			Notify("Locations reloaded");
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("Favorites / Saved");
		DrawLocationList(true);
		ImGui::Spacing();
		ImGui::TextUnformatted("All Locations");
		DrawLocationList(false);

		ImGui::SeparatorText("Recent");
		for (size_t i = 0; i < recent_.size(); ++i) {
			auto& r = recent_[i];
			ImGui::PushID(static_cast<int>(i) + 1000);
			ImGui::Text("%.0f, %.0f, %.0f", r.x, r.y, r.z);
			ImGui::SameLine();
			if (ImGui::SmallButton("Go##tp_recent")) {
				posX_ = r.x; posY_ = r.y; posZ_ = r.z;
				QueueSetpos(r.x, r.y, r.z, false);
				Notify("Queued recent");
			}
			ImGui::PopID();
		}
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"teleport.setpos", "Teleport XYZ", "teleport setpos coords move", FeatureCategory::Teleport, {}});
		out.push_back({"teleport.moveto", "Moveto Ref", "moveto reference teleport", FeatureCategory::Teleport, {}});
		out.push_back({"teleport.locations", "Saved Locations", "locations favorites json warp", FeatureCategory::Teleport, {}});
		out.push_back({"teleport.places", "Quick Places", "primm novac goodsprings coc town", FeatureCategory::Teleport, {}});
	}

	nlohmann::json Serialize() const override
	{
		return {
			{"posX", posX_}, {"posY", posY_}, {"posZ", posZ_},
			{"moveto", movetoBuf_}, {"name", nameBuf_}
		};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		posX_ = j.value("posX", posX_);
		posY_ = j.value("posY", posY_);
		posZ_ = j.value("posZ", posZ_);
		if (j.contains("moveto") && j["moveto"].is_string()) {
			std::snprintf(movetoBuf_, sizeof(movetoBuf_), "%s", j["moveto"].get<std::string>().c_str());
		}
		if (j.contains("name") && j["name"].is_string()) {
			std::snprintf(nameBuf_, sizeof(nameBuf_), "%s", j["name"].get<std::string>().c_str());
		}
	}

private:
	struct PendingWarp {
		bool active = false;
		unsigned fireAtMs = 0;
		int lineCount = 0;
		char lines[4][96]{};
	};

	void BeginQueue()
	{
		// Stop drawing ImGui over the frame that fires coc — prevents freezes.
		Input::Get().SetMenuOpen(false);
		Input::Get().SetSearchOpen(false);
		// Auto companion moveto is OFF by default — opt-in only (was AV'ing the exe).
		if (CompanionFollow::Get().Enabled())
			CompanionFollow::Get().RequestAfterTeleport();
		pending_.active = true;
		pending_.fireAtMs = GetTickCount() + 50;
		pending_.lineCount = 0;
	}

	void PushLine(const char* line)
	{
		if (pending_.lineCount >= 4 || !line) return;
		std::snprintf(pending_.lines[pending_.lineCount], sizeof(pending_.lines[0]), "%s", line);
		++pending_.lineCount;
	}

	void QueueCocOrCmd(const char* line, const char* notifyLabel)
	{
		BeginQueue();
		PushLine(line);
		if (notifyLabel && notifyLabel[0])
			Notify(notifyLabel);
	}

	void QueuePlace(const char* cell, const char* label)
	{
		BeginQueue();
		char line[96];
		std::snprintf(line, sizeof(line), "coc %s", cell);
		PushLine(line);
		Notify(label ? label : cell);
		SFC_LOG("[TELEPORT] queued coc %s", cell);
	}

	void QueueSetpos(float x, float y, float z, bool pushRecent)
	{
		BeginQueue();
		char a[96], b[96], c[96];
		std::snprintf(a, sizeof(a), "player.setpos x %.2f", x);
		std::snprintf(b, sizeof(b), "player.setpos y %.2f", y);
		std::snprintf(c, sizeof(c), "player.setpos z %.2f", z);
		PushLine(a);
		PushLine(b);
		PushLine(c);
		if (pushRecent) PushRecent(x, y, z);
	}

	void DrawPlaceGrid(const char* childId, const PlaceEntry* places, int count)
	{
		ImGui::BeginChild(childId, ImVec2(0, 140), true);
		int col = 0;
		for (int i = 0; i < count; ++i) {
			const auto& p = places[i];
			if (!PlaceLabelMatch(p.label, placeFilter_)) continue;
			ImGui::PushID(p.cell);
			if (ImGui::Button(p.label, ImVec2(150, 0))) {
				QueuePlace(p.cell, p.label);
			}
			ImGui::PopID();
			++col;
			if (col % 3 != 0) ImGui::SameLine();
		}
		ImGui::EndChild();
	}

	void DrawLocationList(bool favoritesOnly)
	{
		for (size_t i = 0; i < locations_.size(); ++i) {
			auto& loc = locations_[i];
			if (favoritesOnly && !loc.favorite) continue;
			if (!favoritesOnly && loc.favorite) continue;
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::Checkbox("##tp_fav", &loc.favorite)) {
				SaveLocations();
			}
			ImGui::SameLine();
			ImGui::Text("%s  (%.0f, %.0f, %.0f)", loc.name.c_str(), loc.x, loc.y, loc.z);
			ImGui::SameLine();
			if (ImGui::SmallButton("Go##tp_go")) {
				posX_ = loc.x; posY_ = loc.y; posZ_ = loc.z;
				QueueSetpos(loc.x, loc.y, loc.z, true);
				Notify("Queued location");
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Del##tp_del")) {
				locations_.erase(locations_.begin() + static_cast<std::ptrdiff_t>(i));
				SaveLocations();
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}
		if (favoritesOnly) {
			bool any = false;
			for (const auto& l : locations_) if (l.favorite) { any = true; break; }
			if (!any) ImGui::TextDisabled("No favorites yet.");
		}
	}

	void PushRecent(float x, float y, float z)
	{
		LocationEntry e;
		e.name = "recent";
		e.x = x; e.y = y; e.z = z;
		recent_.insert(recent_.begin(), e);
		if (recent_.size() > 8) recent_.resize(8);
		SaveLocations();
	}

	void LoadLocations()
	{
		locations_.clear();
		recent_.clear();
		const auto path = LocationsPath();
		std::ifstream in(path);
		if (!in) return;
		try {
			nlohmann::json j;
			in >> j;
			if (j.contains("locations") && j["locations"].is_array()) {
				for (const auto& item : j["locations"]) {
					LocationEntry e;
					e.name = item.value("name", "unnamed");
					e.x = item.value("x", 0.f);
					e.y = item.value("y", 0.f);
					e.z = item.value("z", 0.f);
					e.favorite = item.value("favorite", false);
					locations_.push_back(std::move(e));
				}
			}
			if (j.contains("recent") && j["recent"].is_array()) {
				for (const auto& item : j["recent"]) {
					LocationEntry e;
					e.x = item.value("x", 0.f);
					e.y = item.value("y", 0.f);
					e.z = item.value("z", 0.f);
					recent_.push_back(std::move(e));
				}
			}
		} catch (const std::exception& ex) {
			SFC_WARN("Failed to load locations.json: %s", ex.what());
		}
	}

	void SaveLocations()
	{
		const auto path = LocationsPath();
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
		nlohmann::json j;
		j["locations"] = nlohmann::json::array();
		for (const auto& loc : locations_) {
			j["locations"].push_back({
				{"name", loc.name}, {"x", loc.x}, {"y", loc.y}, {"z", loc.z}, {"favorite", loc.favorite}
			});
		}
		j["recent"] = nlohmann::json::array();
		for (const auto& r : recent_) {
			j["recent"].push_back({{"x", r.x}, {"y", r.y}, {"z", r.z}});
		}
		std::ofstream out(path);
		if (!out) {
			SFC_WARN("Could not write locations.json");
			return;
		}
		out << j.dump(2);
	}

	float posX_ = 0.f, posY_ = 0.f, posZ_ = 0.f;
	char movetoBuf_[16] = "00000000";
	char nameBuf_[64] = "";
	char placeFilter_[64] = "";
	std::vector<LocationEntry> locations_;
	std::vector<LocationEntry> recent_;
	PendingWarp pending_{};
};

std::unique_ptr<IFeature> CreateTeleportFeature()
{
	return std::make_unique<TeleportFeature>();
}

} // namespace sfc
