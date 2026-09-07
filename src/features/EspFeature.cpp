#include "core/Feature.hpp"
#include "core/Config.hpp"
#include "core/GameState.hpp"
#include "core/Log.hpp"
#include "render/WorldToScreen.hpp"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <memory>

namespace sfc {
namespace {

ImU32 ColorForKind(std::uint8_t kind)
{
	switch (kind) {
	case 0: return IM_COL32(255, 180, 40, 255);
	case 2: return IM_COL32(100, 180, 255, 255);
	default: return IM_COL32(80, 255, 120, 255);
	}
}

void BoxExtentsForKind(std::uint8_t kind, float& halfW, float& halfH, float& halfD)
{
	switch (kind) {
	case 0: halfW = 30.f; halfH = 65.f; halfD = 30.f; break;
	case 2: halfW = 45.f; halfH = 80.f; halfD = 15.f; break;
	default: halfW = 22.f; halfH = 22.f; halfD = 22.f; break;
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

		static int cool = 0;
		if (cool-- <= 0) {
			cool = 20;
			SFC_LOG("ESP scan markers=%d enabled=%d",
				static_cast<int>(GameState::Get().Snapshot().nearby.size()), enabled_ ? 1 : 0);
		}
	}

	void DrawMenu() override
	{
		auto& perf = Config::Get().Data().performance;
		ImGui::SeparatorText("Overlay");
		if (ImGui::Checkbox("Enable world box ESP##esp_enable", &enabled_)) {
			Config::Get().Data().performance.espEnabled = enabled_;
			Config::Get().MarkDirty();
		}
		ImGui::Checkbox("Show side list##esp_list", &showList_);
		ImGui::TextWrapped("Close this menu (ESC). You should see amber boxes on NPCs in the world.");

		ImGui::SeparatorText("Categories");
		ImGui::Checkbox("NPCs / Creatures##esp_npc", &showNpcs_);
		ImGui::Checkbox("Loot / Weapons##esp_loot", &showLoot_);
		ImGui::Checkbox("Containers##esp_containers", &showContainers_);
		ImGui::Checkbox("Doors##esp_doors", &showDoors_);

		ImGui::SeparatorText("Limits");
		bool dirty = false;
		dirty |= ImGui::SliderFloat("Max distance##esp_dist", &perf.espMaxDistance, 500.f, 20000.f, "%.0f");
		dirty |= ImGui::SliderInt("Max markers##esp_max", &perf.maxEspMarkers, 8, 128);
		dirty |= ImGui::SliderInt("Scan interval (ms)##esp_scan", &perf.espScanMs, 50, 2000);
		if (dirty) Config::Get().MarkDirty();

		ImGui::Text("Scanned last: %d markers", lastScan_);
		ImGui::Text("Drawn last: %d boxes", lastDrawn_);
	}

	void DrawHud() override
	{
		if (!enabled_) return;

		const auto& markers = GameState::Get().Snapshot().nearby;
		lastScan_ = static_cast<int>(markers.size());
		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		const ImVec2 disp = ImGui::GetIO().DisplaySize;

		int drawn = 0;
		for (const auto& m : markers) {
			float halfW = 0, halfH = 0, halfD = 0;
			BoxExtentsForKind(m.kind, halfW, halfH, halfD);

			float minX, minY, maxX, maxY;
			if (!WorldBoxToScreenRect(m.x, m.y, m.z, halfW, halfH, halfD, minX, minY, maxX, maxY)) {
				// Fallback: single-point marker if full box fails
				ScreenPos sp = WorldToScreen(m.x, m.y, m.z + halfH);
				if (!sp.ok) continue;
				minX = sp.x - 20.f; maxX = sp.x + 20.f;
				minY = sp.y - 40.f; maxY = sp.y + 10.f;
			}

			if (maxX < 0.f || maxY < 0.f || minX > disp.x || minY > disp.y) continue;

			const ImU32 col = ColorForKind(m.kind);
			dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), IM_COL32(0, 0, 0, 180), 0.f, 0, 3.5f);
			dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), col, 0.f, 0, 2.0f);

			char label[96];
			std::snprintf(label, sizeof(label), "%s [%.0f]", m.name.c_str(), m.distance);
			const ImVec2 ts = ImGui::CalcTextSize(label);
			const float lx = minX;
			const float ly = (std::max)(0.f, minY - ts.y - 3.f);
			dl->AddRectFilled(ImVec2(lx - 3.f, ly - 1.f), ImVec2(lx + ts.x + 3.f, ly + ts.y + 1.f), IM_COL32(0, 0, 0, 180));
			dl->AddText(ImVec2(lx, ly), col, label);
			++drawn;
		}
		lastDrawn_ = drawn;

		// Always show status so we know ESP is running even if 0 boxes
		ImGui::SetNextWindowPos(ImVec2(disp.x - 260.f, 48.f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.65f);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
		if (ImGui::Begin("##SFC_ESP", nullptr, flags)) {
			ImGui::TextColored(ImVec4(1.f, 0.75f, 0.2f, 1.f), "ESP ON  boxes:%d  scan:%d", drawn, lastScan_);
			if (drawn == 0 && lastScan_ == 0)
				ImGui::TextDisabled("No targets in cell scan");
			else if (drawn == 0 && lastScan_ > 0)
				ImGui::TextDisabled("Targets found but W2S failed");
			if (showList_) {
				const int show = (std::min)(lastScan_, 8);
				for (int i = 0; i < show; ++i) {
					const auto& m = markers[static_cast<size_t>(i)];
					const char* tag = m.kind == 0 ? "NPC" : (m.kind == 2 ? "DOOR" : "LOOT");
					ImGui::Text("%s %4.0f %s", tag, m.distance, m.name.c_str());
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
			{"enabled", enabled_}, {"showList", showList_},
			{"showNpcs", showNpcs_}, {"showLoot", showLoot_},
			{"showContainers", showContainers_}, {"showDoors", showDoors_}
		};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		enabled_ = j.value("enabled", enabled_);
		showList_ = j.value("showList", showList_);
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
