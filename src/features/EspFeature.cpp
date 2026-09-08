#include "core/Feature.hpp"
#include "core/Config.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/CompanionFollow.hpp"
#include "core/GameState.hpp"
#include "core/Input.hpp"
#include "core/Log.hpp"
#include "core/LootVacuum.hpp"
#include "render/WorldToScreen.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

namespace sfc {
namespace {

constexpr float kUnitsPerFoot = 128.f / 6.f;

float DistFeet(float units) { return units / kUnitsPerFoot; }
float FeetToUnits(float ft) { return ft * kUnitsPerFoot; }

ImU32 ColorForKind(std::uint8_t kind, int alpha = 255)
{
	alpha = (std::max)(40, (std::min)(255, alpha));
	switch (kind) {
	case 0: return IM_COL32(255, 96, 72, alpha);   // NPC
	case 2: return IM_COL32(96, 176, 255, alpha);  // door
	case 3: return IM_COL32(255, 208, 72, alpha);  // container
	default: return IM_COL32(96, 255, 140, alpha); // loot
	}
}

void BoxExtentsForKind(std::uint8_t kind, float& halfW, float& halfH, float& halfD)
{
	switch (kind) {
	case 0: halfW = 24.f; halfH = 52.f; halfD = 24.f; break;  // NPC
	case 2: halfW = 36.f; halfH = 56.f; halfD = 14.f; break;  // door
	case 3: halfW = 22.f; halfH = 20.f; halfD = 22.f; break;  // container
	default: halfW = 16.f; halfH = 14.f; halfD = 16.f; break; // loot
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

const char* DisplayName(const NearbyMarker& m)
{
	if (m.name.empty() || m.name == "?" || m.name == "???")
		return TagForKind(m.kind);
	return m.name.c_str();
}

int AlphaForDistance(float feet)
{
	if (feet < 250.f) return 255;
	if (feet < 800.f) return 220;
	if (feet < 2000.f) return 185;
	return 150;
}

void EnforceMinBox(float& minX, float& minY, float& maxX, float& maxY, float minSize)
{
	float w = maxX - minX;
	float h = maxY - minY;
	const float cx = 0.5f * (minX + maxX);
	const float cy = 0.5f * (minY + maxY);
	if (w < minSize) {
		minX = cx - minSize * 0.5f;
		maxX = cx + minSize * 0.5f;
	}
	if (h < minSize) {
		minY = cy - minSize * 0.5f;
		maxY = cy + minSize * 0.5f;
	}
}

void DrawCornerBox(ImDrawList* dl, float minX, float minY, float maxX, float maxY, ImU32 col, float thick)
{
	const float w = maxX - minX;
	const float h = maxY - minY;
	float arm = (std::min)(w, h) * 0.28f;
	if (arm < 6.f) arm = 6.f;
	if (arm > 18.f) arm = 18.f;

	auto corner = [&](float x0, float y0, float dx, float dy) {
		dl->AddLine(ImVec2(x0, y0), ImVec2(x0 + dx, y0), col, thick);
		dl->AddLine(ImVec2(x0, y0), ImVec2(x0, y0 + dy), col, thick);
	};
	corner(minX, minY, arm, arm);
	corner(maxX, minY, -arm, arm);
	corner(minX, maxY, arm, -arm);
	corner(maxX, maxY, -arm, -arm);
}

} // namespace

class EspFeature : public IFeature {
public:
	const char* Id() const override { return "esp"; }
	const char* Name() const override { return "ESP"; }
	FeatureCategory Category() const override { return FeatureCategory::Esp; }

	void Tick(float dt) override
	{
		if (!enabled_) return;
		auto& perf = Config::Get().Data().performance;
		GameState::Get().WantNearbyScan(
			perf.espMaxDistance,
			perf.maxEspMarkers,
			showNpcs_,
			showLoot_ || showContainers_,
			showDoors_);
		lastScan_ = static_cast<int>(GameState::Get().Snapshot().nearby.size());
		(void)dt;
	}

	void DrawMenu() override
	{
		auto& perf = Config::Get().Data().performance;
		ImGui::SeparatorText("Overlay");
		ImGui::TextWrapped("World boxes for NPCs, loot, doors, and containers. Heal/Revive → NPCs tab.");
		if (ImGui::Checkbox("Enable ESP##esp_enable", &enabled_)) {
			Config::Get().Data().performance.espEnabled = enabled_;
			Config::Get().MarkDirty();
			if (enabled_) {
				showNpcs_ = true;
				showDoors_ = true;
				showLoot_ = true;
				showContainers_ = true;
				showList_ = true;
				showLabels_ = true;
			}
		}
		ImGui::Checkbox("Show side list##esp_list", &showList_);
		ImGui::Checkbox("Draw world boxes##esp_boxes", &drawBoxes_);
		ImGui::Checkbox("Draw labels##esp_labels", &showLabels_);
		ImGui::Checkbox("Corner style##esp_corners", &cornerStyle_);

		ImGui::SeparatorText("What to show");
		ImGui::Checkbox("NPCs / enemies##esp_npc", &showNpcs_);
		ImGui::Checkbox("Doors##esp_doors", &showDoors_);
		ImGui::Checkbox("Ground loot##esp_loot", &showLoot_);
		ImGui::Checkbox("Containers / chests##esp_containers", &showContainers_);

		ImGui::SeparatorText("Loot / Grab");
		ImGui::TextWrapped("Vacuum, Smart Grab, and Grab All live in the GRAB tab now.");
		if (ImGui::Button("Open tip: use GRAB tab##esp_grab_tip")) {
			Notify("Switch left nav to GRAB");
		}

		ImGui::SeparatorText("Limits");
		bool dirty = false;
		float rangeFt = DistFeet(perf.espMaxDistance);
		if (ImGui::SliderFloat("Max range##esp_dist_ft", &rangeFt, 100.f, 5000.f, "%.0f ft")) {
			perf.espMaxDistance = FeetToUnits(rangeFt);
			dirty = true;
		}
		ImGui::TextDisabled("Loaded cells only — far unloaded desert will not appear.");
		dirty |= ImGui::SliderInt("Max markers##esp_max", &perf.maxEspMarkers, 32, 512);
		dirty |= ImGui::SliderInt("Scan interval (ms)##esp_scan", &perf.espScanMs, 100, 2000);
		if (dirty) Config::Get().MarkDirty();

		if (ImGui::Button("Scan now##esp_scan_btn")) {
			auto& perf2 = Config::Get().Data().performance;
			GameState::Get().WantNearbyScan(
				perf2.espMaxDistance,
				perf2.maxEspMarkers,
				showNpcs_,
				showLoot_ || showContainers_,
				showDoors_);
			GameState::Get().FlushNearbyScanNow();
			lastScan_ = static_cast<int>(GameState::Get().Snapshot().nearby.size());
		}
		ImGui::SameLine();
		ImGui::Text("scan %d  |  boxes %d  |  miss %d", lastScan_, lastDrawn_, lastW2sFail_ + lastRejected_);

		ImGui::SeparatorText("Nearby");
		const auto& markers = GameState::Get().Snapshot().nearby;
		ImGui::BeginChild("##esp_menu_list", ImVec2(0, 260), true);
		if (markers.empty()) {
			ImGui::TextDisabled("Empty — enable ESP or hit Scan now.");
		} else {
			int shown = 0;
			for (const auto& m : markers) {
				if (!KindWanted(m.kind, showNpcs_, showLoot_, showContainers_, showDoors_)) continue;
				ImGui::PushID(static_cast<int>(m.refId ? m.refId : (shown + 1) * 17));
				ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ColorForKind(m.kind)),
					"%-5s %4.0fft  %s", TagForKind(m.kind), DistFeet(m.distance), DisplayName(m));
				if (m.refId != 0) {
					if (ImGui::SmallButton("Go")) {
						ConsoleBridge::Get().Runf("player.moveto %08X", m.refId);
						Notify("Moving to target");
					}
					if (m.kind == 0) {
						ImGui::SameLine();
						if (ImGui::SmallButton("Bring")) {
							CompanionFollow::Get().BringRef(m.refId);
							Notify("Bring sent");
						}
						ImGui::SameLine();
						if (ImGui::SmallButton("Heal")) {
							ConsoleBridge::Get().Runf("\"%08X\".resethealth", m.refId);
							Notify("Heal sent");
						}
					} else {
						if (m.kind == 1 || m.kind == 3) {
							ImGui::SameLine();
							if (ImGui::SmallButton("Loot")) {
								if (m.kind == 1) {
									ConsoleBridge::Get().Runf("\"%08X\".Activate player 1", m.refId);
									ConsoleBridge::Get().Runf("\"%08X\".Activate player", m.refId);
									Notify("Activate (pickup)");
								} else {
									ConsoleBridge::Get().Runf("\"%08X\".RemoveAllItems player", m.refId);
									Notify("Transferred to you");
								}
							}
						}
						ImGui::SameLine();
						if (ImGui::SmallButton("Pull")) {
							ConsoleBridge::Get().Runf("\"%08X\".moveto player", m.refId);
							Notify("Moved to feet (not inventory)");
						}
					}
				}
				ImGui::PopID();
				if (++shown >= 28) break;
			}
		}
		ImGui::EndChild();
		ImGui::TextDisabled("Go = you to them | Loot = inventory transfer | Pull = moveto feet only");
	}

	void DrawHud() override
	{
		if (!enabled_) return;

		const auto& markers = GameState::Get().Snapshot().nearby;
		lastScan_ = static_cast<int>(markers.size());
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
					ScreenPos sp = WorldToScreen(m.x, m.y, m.z + halfH);
					if (!sp.ok) {
						++w2sFail;
						continue;
					}
					++w2sOk;
					const float s = (m.kind == 0) ? 22.f : 14.f;
					minX = sp.x - s; maxX = sp.x + s;
					minY = sp.y - s * 1.6f; maxY = sp.y + s * 0.4f;
				} else {
					++w2sOk;
				}

				if (maxX < 2.f || maxY < 2.f || minX > disp.x - 2.f || minY > disp.y - 2.f) {
					++rejected;
					continue;
				}

				EnforceMinBox(minX, minY, maxX, maxY, m.kind == 0 ? 18.f : 12.f);
				minX = (std::max)(0.f, minX);
				minY = (std::max)(0.f, minY);
				maxX = (std::min)(disp.x, maxX);
				maxY = (std::min)(disp.y, maxY);

				const float feet = DistFeet(m.distance);
				const int a = AlphaForDistance(feet);
				const ImU32 col = ColorForKind(m.kind, a);
				const ImU32 outline = IM_COL32(0, 0, 0, (std::min)(220, a));

				if (cornerStyle_) {
					DrawCornerBox(dl, minX, minY, maxX, maxY, outline, 3.4f);
					DrawCornerBox(dl, minX, minY, maxX, maxY, col, 2.0f);
				} else {
					dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), outline, 0.f, 0, 3.2f);
					dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), col, 0.f, 0, 1.8f);
				}

				const float mx = 0.5f * (minX + maxX);
				const float my = 0.5f * (minY + maxY);
				dl->AddLine(ImVec2(mx - 4.f, my), ImVec2(mx + 4.f, my), col, 1.5f);
				dl->AddLine(ImVec2(mx, my - 4.f), ImVec2(mx, my + 4.f), col, 1.5f);

				if (showLabels_) {
					char label[96];
					const char* name = DisplayName(m);
					// Keep labels short so they don't smear across the sky.
					if (std::strlen(name) > 22)
						std::snprintf(label, sizeof(label), "%.18s… %.0fft", name, feet);
					else
						std::snprintf(label, sizeof(label), "%s  %.0fft", name, feet);

					const ImVec2 ts = ImGui::CalcTextSize(label);
					float lx = minX;
					float ly = minY - ts.y - 4.f;
					if (ly < 2.f) ly = maxY + 3.f;
					lx = (std::max)(2.f, (std::min)(disp.x - ts.x - 2.f, lx));
					ly = (std::max)(2.f, (std::min)(disp.y - ts.y - 2.f, ly));
					dl->AddRectFilled(ImVec2(lx - 3.f, ly - 1.f), ImVec2(lx + ts.x + 3.f, ly + ts.y + 1.f),
						IM_COL32(0, 0, 0, (std::min)(200, a)));
					dl->AddText(ImVec2(lx, ly), col, label);
				}
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
			cool = 180;
			SFC_LOG("ESP boxes=%d scan=%d fail=%d reject=%d", drawn, lastScan_, w2sFail, rejected);
		}

		if (!showList_ && drawn > 0) return;

		ImGui::SetNextWindowPos(ImVec2(disp.x - 268.f, 44.f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.62f);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
		if (!Input::Get().MenuOpen())
			flags |= ImGuiWindowFlags_NoInputs;

		if (ImGui::Begin("##SFC_ESP", nullptr, flags)) {
			ImGui::TextColored(ImVec4(1.f, 0.78f, 0.25f, 1.f), "ESP  %d / %d", drawn, lastScan_);
			if (drawn == 0 && lastScan_ == 0)
				ImGui::TextDisabled("No targets in range");
			else if (drawn == 0 && lastScan_ > 0)
				ImGui::TextDisabled("Projected off-view / behind camera");
			if (showList_) {
				int shown = 0;
				for (const auto& m : markers) {
					if (!KindWanted(m.kind, showNpcs_, showLoot_, showContainers_, showDoors_)) continue;
					ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ColorForKind(m.kind)),
						"%-5s %4.0fft  %s", TagForKind(m.kind), DistFeet(m.distance), DisplayName(m));
					if (++shown >= 12) break;
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
			{"showLabels", showLabels_}, {"cornerStyle", cornerStyle_},
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
		showLabels_ = j.value("showLabels", showLabels_);
		cornerStyle_ = j.value("cornerStyle", cornerStyle_);
		showNpcs_ = j.value("showNpcs", showNpcs_);
		showLoot_ = j.value("showLoot", showLoot_);
		showContainers_ = j.value("showContainers", showContainers_);
		showDoors_ = j.value("showDoors", showDoors_);
		// Legacy presets stored vacuum under esp — still apply.
		LootVacuum::Get().Deserialize(j);
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
	bool showLabels_ = true;
	bool cornerStyle_ = true;
	bool showNpcs_ = true;
	bool showLoot_ = true;
	bool showContainers_ = true;
	bool showDoors_ = true;
	float accumMs_ = 0.f; // unused; nearby scan is merged in GameState
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
