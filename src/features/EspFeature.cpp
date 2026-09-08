#include "core/Feature.hpp"
#include "core/Config.hpp"
#include "core/GameState.hpp"
#include "core/Input.hpp"
#include "core/Log.hpp"
#include "render/WorldToScreen.hpp"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <memory>

namespace sfc {
namespace {

// Bethesda units: 128 u = 6 ft
float DistFeet(float units) { return units * (6.f / 128.f); }

ImU32 ColorForKind(std::uint8_t kind)
{
	switch (kind) {
	case 0: return IM_COL32(255, 90, 70, 255);   // NPC / enemy — red-orange
	case 2: return IM_COL32(90, 170, 255, 255);  // door — blue
	case 3: return IM_COL32(255, 210, 80, 255);  // container — gold
	default: return IM_COL32(90, 255, 140, 255); // ground loot — green
	}
}

void BoxExtentsForKind(std::uint8_t kind, float& halfW, float& halfH, float& halfD)
{
	switch (kind) {
	case 0: halfW = 22.f; halfH = 48.f; halfD = 22.f; break;  // NPC
	case 2: halfW = 35.f; halfH = 55.f; halfD = 12.f; break;  // door
	case 3: halfW = 20.f; halfH = 18.f; halfD = 20.f; break;  // container
	default: halfW = 14.f; halfH = 12.f; halfD = 14.f; break; // loot
	}
}

const char* TagForKind(std::uint8_t kind)
{
	switch (kind) {
	case 0: return "NPC";
	case 2: return "DOOR";
	case 3: return "CHEST";
	default: return "LOOT";
	}
}

bool KindWanted(std::uint8_t kind, bool npcs, bool loot, bool containers, bool doors)
{
	switch (kind) {
	case 0: return npcs;
	case 1: return loot;
	case 2: return doors;
	case 3: return containers || loot;
	default: return false;
	}
}

} // namespace

class EspFeature : public IFeature {
public:
	const char* Id() const override { return "esp"; }
	const char* Name() const override { return "ESP"; }
	FeatureCategory Category() const override { return FeatureCategory::Esp; }

	void Tick(float dt) override
	{
		if (!enabled_) {
			if (!GameState::Get().Snapshot().nearby.empty())
				GameState::Get().ScanNearby(0.f, 0, false, false, false);
			return;
		}
		accumMs_ += dt * 1000.f;
		const int interval = Config::Get().Data().performance.espScanMs;
		if (accumMs_ < static_cast<float>(interval > 16 ? interval : 16)) return;
		accumMs_ = 0.f;

		auto& perf = Config::Get().Data().performance;
		GameState::Get().ScanNearby(
			perf.espMaxDistance,
			perf.maxEspMarkers,
			showNpcs_,
			showLoot_ || showContainers_,
			showDoors_);

		lastScan_ = static_cast<int>(GameState::Get().Snapshot().nearby.size());
		static int cool = 0;
		if (cool-- <= 0) {
			cool = 20;
			SFC_LOG("ESP scan markers=%d enabled=%d", lastScan_, enabled_ ? 1 : 0);
		}
	}

	void DrawMenu() override
	{
		auto& perf = Config::Get().Data().performance;
		ImGui::SeparatorText("Overlay");
		ImGui::TextWrapped(
			"Boxes = in front of you. Triangles on the screen edge = behind you / through walls / off-angle. "
			"Heal/Revive → NPCs tab → Scan area.");
		if (ImGui::Checkbox("Enable ESP##esp_enable", &enabled_)) {
			Config::Get().Data().performance.espEnabled = enabled_;
			Config::Get().MarkDirty();
			if (enabled_) {
				showNpcs_ = true;
				showDoors_ = true;
				showLoot_ = true;
				showContainers_ = true;
				showList_ = true;
			}
		}
		ImGui::Checkbox("Show side list##esp_list", &showList_);
		ImGui::Checkbox("Draw world boxes##esp_boxes", &drawBoxes_);

		ImGui::SeparatorText("What to show");
		ImGui::Checkbox("NPCs / enemies##esp_npc", &showNpcs_);
		ImGui::Checkbox("Doors / places you can enter##esp_doors", &showDoors_);
		ImGui::Checkbox("Loot you can pick up##esp_loot", &showLoot_);
		ImGui::Checkbox("Containers / chests##esp_containers", &showContainers_);

		ImGui::SeparatorText("Limits");
		bool dirty = false;
		dirty |= ImGui::SliderFloat("Max distance##esp_dist", &perf.espMaxDistance, 500.f, 12000.f, "%.0f");
		dirty |= ImGui::SliderInt("Max markers##esp_max", &perf.maxEspMarkers, 8, 128);
		dirty |= ImGui::SliderInt("Scan interval (ms)##esp_scan", &perf.espScanMs, 100, 2000);
		if (dirty) Config::Get().MarkDirty();

		if (ImGui::Button("Scan now##esp_scan_btn")) {
			GameState::Get().ScanNearby(
				perf.espMaxDistance,
				perf.maxEspMarkers,
				showNpcs_,
				showLoot_ || showContainers_,
				showDoors_);
			lastScan_ = static_cast<int>(GameState::Get().Snapshot().nearby.size());
		}
		ImGui::SameLine();
		ImGui::Text("Scanned: %d   Boxes drawn last: %d", lastScan_, lastDrawn_);
		if (lastScan_ == 0 && enabled_)
			ImGui::TextDisabled("Nothing in range — walk near NPCs/doors and Scan.");
		else if (lastScan_ > 0 && lastDrawn_ == 0 && drawBoxes_)
			ImGui::TextDisabled("Scan OK — if no boxes, use the list (W2S may fail).");

		ImGui::SeparatorText("Nearby right now");
		const auto& markers = GameState::Get().Snapshot().nearby;
		ImGui::BeginChild("##esp_menu_list", ImVec2(0, 220), true);
		if (markers.empty()) {
			ImGui::TextDisabled("Empty — enable ESP or hit Scan now.");
		} else {
			int shown = 0;
			for (const auto& m : markers) {
				if (!KindWanted(m.kind, showNpcs_, showLoot_, showContainers_, showDoors_)) continue;
				ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ColorForKind(m.kind)),
					"%-5s %4.0fft  %s", TagForKind(m.kind), DistFeet(m.distance), m.name.c_str());
				if (++shown >= 24) break;
			}
		}
		ImGui::EndChild();
		ImGui::TextDisabled("Heal / Revive / Kill → NPCs tab (uses this same scan).");
	}

	void DrawHud() override
	{
		if (!enabled_) return;

		const auto& markers = GameState::Get().Snapshot().nearby;
		lastScan_ = static_cast<int>(markers.size());
		// Foreground so boxes sit above the game HUD / our other windows.
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		const ImVec2 disp = ImGui::GetIO().DisplaySize;

		int drawn = 0;
		int edgeDrawn = 0;
		if (drawBoxes_) {
			for (const auto& m : markers) {
				if (!KindWanted(m.kind, showNpcs_, showLoot_, showContainers_, showDoors_)) continue;
				float halfW = 0, halfH = 0, halfD = 0;
				BoxExtentsForKind(m.kind, halfW, halfH, halfD);

				const ImU32 col = ColorForKind(m.kind);
				float minX, minY, maxX, maxY;
				bool got = WorldBoxToScreenRect(m.x, m.y, m.z, halfW, halfH, halfD, minX, minY, maxX, maxY);
				bool onScreenBox = false;
				if (got) {
					onScreenBox = !(maxX < 2.f || maxY < 2.f || minX > disp.x - 2.f || minY > disp.y - 2.f);
				}

				if (onScreenBox) {
					minX = (std::max)(0.f, minX);
					minY = (std::max)(0.f, minY);
					maxX = (std::min)(disp.x, maxX);
					maxY = (std::min)(disp.y, maxY);

					dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), IM_COL32(0, 0, 0, 220), 0.f, 0, 4.0f);
					dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), col, 0.f, 0, 2.5f);
					const float mx = 0.5f * (minX + maxX);
					const float my = 0.5f * (minY + maxY);
					dl->AddLine(ImVec2(mx - 6.f, my), ImVec2(mx + 6.f, my), col, 2.f);
					dl->AddLine(ImVec2(mx, my - 6.f), ImVec2(mx, my + 6.f), col, 2.f);

					char label[96];
					std::snprintf(label, sizeof(label), "%s [%.0fft]", m.name.c_str(), DistFeet(m.distance));
					const ImVec2 ts = ImGui::CalcTextSize(label);
					const float lx = minX;
					const float ly = (std::max)(0.f, minY - ts.y - 3.f);
					dl->AddRectFilled(ImVec2(lx - 3.f, ly - 1.f), ImVec2(lx + ts.x + 3.f, ly + ts.y + 1.f), IM_COL32(0, 0, 0, 200));
					dl->AddText(ImVec2(lx, ly), col, label);
					++drawn;
					continue;
				}

				// Through walls / behind you / off to the side: edge ping (not dropped).
				float sx = 0.f, sy = 0.f;
				bool onScreen = false;
				if (!ProjectEspPoint(m.x, m.y, m.z + halfH, sx, sy, onScreen))
					continue;
				if (onScreen) {
					minX = sx - 18.f; maxX = sx + 18.f;
					minY = sy - 48.f; maxY = sy + 8.f;
					dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), col, 0.f, 0, 2.0f);
					++drawn;
				} else {
					const float s = 10.f;
					dl->AddTriangleFilled(
						ImVec2(sx, sy - s),
						ImVec2(sx - s, sy + s * 0.6f),
						ImVec2(sx + s, sy + s * 0.6f),
						col);
					dl->AddCircle(ImVec2(sx, sy), 3.f, IM_COL32(0, 0, 0, 220), 8, 2.f);
					char label[96];
					std::snprintf(label, sizeof(label), "%s %.0fft", TagForKind(m.kind), DistFeet(m.distance));
					const ImVec2 ts = ImGui::CalcTextSize(label);
					float lx = sx - ts.x * 0.5f;
					float ly = sy + 12.f;
					lx = (std::max)(2.f, (std::min)(disp.x - ts.x - 2.f, lx));
					ly = (std::max)(2.f, (std::min)(disp.y - ts.y - 2.f, ly));
					dl->AddRectFilled(ImVec2(lx - 2.f, ly - 1.f), ImVec2(lx + ts.x + 2.f, ly + ts.y + 1.f), IM_COL32(0, 0, 0, 190));
					dl->AddText(ImVec2(lx, ly), col, label);
					++edgeDrawn;
				}
			}
		}
		lastDrawn_ = drawn + edgeDrawn;

		static int cool = 0;
		if (cool-- <= 0) {
			cool = 120;
			SFC_LOG("ESP draw boxes=%d edge=%d scan=%d", drawn, edgeDrawn, lastScan_);
		}

		if (!showList_ && (drawn + edgeDrawn) > 0) return;

		ImGui::SetNextWindowPos(ImVec2(disp.x - 280.f, 48.f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.70f);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
		if (!Input::Get().MenuOpen())
			flags |= ImGuiWindowFlags_NoInputs;

		if (ImGui::Begin("##SFC_ESP", nullptr, flags)) {
			ImGui::TextColored(ImVec4(1.f, 0.75f, 0.2f, 1.f), "ESP ON  boxes:%d  edge:%d  scan:%d",
				drawn, edgeDrawn, lastScan_);
			if (drawn + edgeDrawn == 0 && lastScan_ == 0)
				ImGui::TextDisabled("No targets in scan");
			else if (drawn + edgeDrawn == 0 && lastScan_ > 0)
				ImGui::TextDisabled("Scan OK — projection failed");
			if (showList_) {
				int shown = 0;
				for (const auto& m : markers) {
					if (!KindWanted(m.kind, showNpcs_, showLoot_, showContainers_, showDoors_)) continue;
					ImGui::Text("%-5s %4.0fft  %s", TagForKind(m.kind), DistFeet(m.distance), m.name.c_str());
					if (++shown >= 14) break;
				}
			}
		}
		ImGui::End();
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"esp.toggle", "ESP Boxes", "esp wallhack markers npc loot boxes", FeatureCategory::Esp, {}});
	}

	nlohmann::json Serialize() const override
	{
		return {
			{"enabled", enabled_}, {"showList", showList_}, {"drawBoxes", drawBoxes_},
			{"showNpcs", showNpcs_}, {"showLoot", showLoot_},
			{"showContainers", showContainers_}, {"showDoors", showDoors_}
		};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		enabled_ = j.value("enabled", enabled_);
		showList_ = j.value("showList", showList_);
		drawBoxes_ = j.value("drawBoxes", drawBoxes_);
		showNpcs_ = j.value("showNpcs", showNpcs_);
		showLoot_ = j.value("showLoot", showLoot_);
		showContainers_ = j.value("showContainers", showContainers_);
		showDoors_ = j.value("showDoors", showDoors_);
	}

	bool Init() override
	{
		enabled_ = Config::Get().Data().performance.espEnabled;
		return true;
	}

private:
	bool enabled_ = false;
	bool showList_ = true;
	bool drawBoxes_ = true;
	bool showNpcs_ = true;
	bool showLoot_ = true;
	bool showContainers_ = true;
	bool showDoors_ = true;
	float accumMs_ = 0.f;
	int lastScan_ = 0;
	int lastDrawn_ = 0;
};

std::unique_ptr<IFeature> CreateEspFeature()
{
	return std::make_unique<EspFeature>();
}

} // namespace sfc
