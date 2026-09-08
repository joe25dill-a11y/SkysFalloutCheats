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
			"Boxes use live FOV + 8-corner world projection. Buildings do not hide overlays. "
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
		// UI in feet; stored config is still game units.
		float rangeFt = perf.espMaxDistance * (6.f / 128.f);
		if (ImGui::SliderFloat("Max range (feet)##esp_dist_ft", &rangeFt, 100.f, 2000.f, "%.0f ft")) {
			perf.espMaxDistance = rangeFt * (128.f / 6.f);
			dirty = true;
		}
		ImGui::TextDisabled("%.0f game units  |  loot scans nearby loaded cells (not the whole map)",
			perf.espMaxDistance);
		dirty |= ImGui::SliderInt("Max markers##esp_max", &perf.maxEspMarkers, 32, 512);
		dirty |= ImGui::SliderInt("Scan interval (ms)##esp_scan", &perf.espScanMs, 100, 2000);
		if (dirty) Config::Get().MarkDirty();
		ImGui::TextWrapped(
			"Shows containers/loot/doors in your cell and neighboring loaded cells within range. "
			"The game must have the cell loaded — empty desert far away will not ESP until you get closer.");

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
		ImGui::Text("Scan:%d  W2S ok:%d fail:%d  reject:%d  boxes:%d",
			lastScan_, lastW2sOk_, lastW2sFail_, lastRejected_, lastDrawn_);
		EspCamInfo cam{};
		if (GetEspCamInfo(cam))
			ImGui::TextDisabled("Cam fov=%.1f° src=%d fovSrc=%d  (ALT should change fov)",
				cam.fovDeg, cam.source, cam.fovSource);
		if (lastScan_ == 0 && enabled_)
			ImGui::TextDisabled("Nothing in range — walk near NPCs/doors and Scan.");
		else if (lastScan_ > 0 && lastDrawn_ == 0 && drawBoxes_)
			ImGui::TextDisabled("Scan OK — W2S failing or all behind camera.");

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
		int w2sOk = 0;
		int w2sFail = 0;
		int rejected = 0;
		int candidates = 0;
		if (drawBoxes_) {
			for (const auto& m : markers) {
				if (!KindWanted(m.kind, showNpcs_, showLoot_, showContainers_, showDoors_)) continue;
				++candidates;
				float halfW = 0, halfH = 0, halfD = 0;
				BoxExtentsForKind(m.kind, halfW, halfH, halfD);

				float minX, minY, maxX, maxY;
				bool got = WorldBoxToScreenRect(m.x, m.y, m.z, halfW, halfH, halfD, minX, minY, maxX, maxY);
				if (!got) {
					// Center point probe so we can separate "all corners failed" vs box math.
					ScreenPos sp = WorldToScreen(m.x, m.y, m.z + halfH);
					if (!sp.ok) {
						++w2sFail;
						continue;
					}
					++w2sOk;
					minX = sp.x - 18.f; maxX = sp.x + 18.f;
					minY = sp.y - 48.f; maxY = sp.y + 8.f;
				} else {
					++w2sOk;
				}

				// Fully off-screen → count as rejected (still projected). Partial stays.
				if (maxX < 2.f || maxY < 2.f || minX > disp.x - 2.f || minY > disp.y - 2.f) {
					++rejected;
					continue;
				}
				minX = (std::max)(0.f, minX);
				minY = (std::max)(0.f, minY);
				maxX = (std::min)(disp.x, maxX);
				maxY = (std::min)(disp.y, maxY);

				const ImU32 col = ColorForKind(m.kind);
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
			}
		}
		lastDrawn_ = drawn;
		lastW2sOk_ = w2sOk;
		lastW2sFail_ = w2sFail;
		lastRejected_ = rejected;
		lastCandidates_ = candidates;

		static int cool = 0;
		if (cool-- <= 0) {
			cool = 120;
			EspCamInfo cam{};
			GetEspCamInfo(cam);
			SFC_LOG("ESP cand=%d w2sOk=%d w2sFail=%d reject=%d boxes=%d scan=%d fov=%.1f fovSrc=%d",
				candidates, w2sOk, w2sFail, rejected, drawn, lastScan_, cam.fovDeg, cam.fovSource);
		}

		if (!showList_ && drawn > 0) return;

		ImGui::SetNextWindowPos(ImVec2(disp.x - 300.f, 48.f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.70f);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
		if (!Input::Get().MenuOpen())
			flags |= ImGuiWindowFlags_NoInputs;

		if (ImGui::Begin("##SFC_ESP", nullptr, flags)) {
			ImGui::TextColored(ImVec4(1.f, 0.75f, 0.2f, 1.f),
				"ESP  boxes:%d  scan:%d", drawn, lastScan_);
			ImGui::TextDisabled("W2S ok:%d fail:%d  offscreen:%d  cand:%d",
				w2sOk, w2sFail, rejected, candidates);
			if (drawn == 0 && lastScan_ == 0)
				ImGui::TextDisabled("No targets in scan");
			else if (drawn == 0 && lastScan_ > 0)
				ImGui::TextDisabled("Behind cam / W2S fail — not geometry occlusion");
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
	int lastW2sOk_ = 0;
	int lastW2sFail_ = 0;
	int lastRejected_ = 0;
	int lastCandidates_ = 0;
};

std::unique_ptr<IFeature> CreateEspFeature()
{
	return std::make_unique<EspFeature>();
}

} // namespace sfc
