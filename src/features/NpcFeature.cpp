#include "core/Feature.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/CompanionFollow.hpp"
#include "core/GameState.hpp"
#include "core/Config.hpp"
#include "core/Input.hpp"
#include "core/Log.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace sfc {
namespace {

struct Companion {
	const char* name;
	const char* baseId;
	std::uint32_t refId;
};

const Companion kCompanions[] = {
	{"Craig Boone", "00092BD2", 0x0010D8E9},
	{"Rose of Sharon Cassidy", "00133FDD", 0x00133FDE},
	{"Veronica", "000E32AA", 0x000E32A9},
	{"Lily Bowen", "0013D834", 0x0013D8A5},
	{"Raul Tejada", "000E60EF", 0x000E6105},
	{"Arcade Gannon", "0010C767", 0x0010D8EB},
	{"Rex", "00118E71", 0x00118A8A},
	{"ED-E", "0010C769", 0x0010C76A},
};

void RunOnRef(std::uint32_t refId, const char* cmdSuffix)
{
	if (refId == 0 || !cmdSuffix) return;
	ConsoleBridge::Get().Runf("\"%08X\".%s", refId, cmdSuffix);
}

} // namespace

class NpcFeature : public IFeature {
public:
	const char* Id() const override { return "npc"; }
	const char* Name() const override { return "NPCs"; }
	FeatureCategory Category() const override { return FeatureCategory::Npcs; }

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
		auto& perf = Config::Get().Data().performance;

		ImGui::SeparatorText("Nearby (scan — no console click needed)");
		ImGui::TextWrapped(
			"Scans actors around you and lists them with Heal / Revive / Kill / Bring. "
			"No ~ console selecting.");
		ImGui::SliderFloat("Scan range##npc_scan_range", &scanRange_, 500.f, 8000.f, "%.0f");
		if (ImGui::Button("Scan area now##npc_scan")) {
			GameState::Get().ScanNearby(scanRange_, 48, true, false, false);
			nearby_ = GameState::Get().Snapshot().nearby;
			Notify(nearby_.empty() ? "No NPCs in range" : "Nearby list updated");
		}
		ImGui::SameLine();
		ImGui::Checkbox("Auto-refresh while menu open##npc_auto_scan", &autoScan_);
		if (autoScan_) {
			autoScanAccum_ += ImGui::GetIO().DeltaTime;
			if (autoScanAccum_ >= 1.25f) {
				autoScanAccum_ = 0.f;
				GameState::Get().ScanNearby(scanRange_, 48, true, false, false);
				nearby_ = GameState::Get().Snapshot().nearby;
			}
		}

		ImGui::BeginChild("##npc_nearby", ImVec2(0, 260), true);
		if (nearby_.empty()) {
			ImGui::TextDisabled("Empty — stand near NPCs and hit Scan.");
		}
		int row = 0;
		for (const auto& m : nearby_) {
			if (m.kind != 0) continue; // NPCs only here
			if (m.refId == 0) continue;
			ImGui::PushID(static_cast<int>(m.refId) ^ (row << 16));
			ImGui::Text("%.0fft  %s", m.distance * (6.f / 128.f), m.name.c_str());
			ImGui::SameLine(280.f);
			ImGui::TextDisabled("%08X", m.refId);
			ImGui::SameLine(370.f);
			if (ImGui::SmallButton("Heal")) {
				RunOnRef(m.refId, "restoreav health 99999");
				RunOnRef(m.refId, "restoreav actionpoints 99999");
				Notify("Heal sent");
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Revive")) {
				Input::Get().SetMenuOpen(false);
				RunOnRef(m.refId, "resurrect 1");
				Notify("Revive queued");
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Kill")) {
				RunOnRef(m.refId, "kill");
				Notify("Kill sent");
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Bring")) {
				CompanionFollow::Get().BringRef(m.refId);
				Notify("Bring sent");
			}
			ImGui::PopID();
			++row;
		}
		ImGui::EndChild();

		ImGui::SeparatorText("Companions (story)");
		ImGui::TextWrapped("Bring uses the ORIGINAL companion ref (not a clone).");
		if (ImGui::Button("Bring all teammates here##npc_bring_all")) {
			const int n = CompanionFollow::Get().BringTeammatesNow();
			Notify(n > 0 ? "Teammates moving" : "No teammates found");
		}
		ImGui::SameLine();
		{
			bool follow = CompanionFollow::Get().Enabled();
			if (ImGui::Checkbox("Auto-follow SFC teleports##npc_auto", &follow))
				CompanionFollow::Get().SetEnabled(follow);
		}

		ImGui::BeginChild("##npc_companions", ImVec2(0, 180), true);
		for (const auto& c : kCompanions) {
			ImGui::PushID(c.baseId);
			ImGui::TextUnformatted(c.name);
			ImGui::SameLine(220.f);
			if (ImGui::SmallButton("Bring##c")) {
				CompanionFollow::Get().BringRef(c.refId);
				Notify(c.name);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Heal##c")) {
				RunOnRef(c.refId, "restoreav health 99999");
				RunOnRef(c.refId, "restoreav actionpoints 99999");
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Revive##c")) {
				Input::Get().SetMenuOpen(false);
				RunOnRef(c.refId, "resurrect 1");
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Essential##c")) {
				console.Runf("setessential %s 1", c.baseId);
			}
			ImGui::PopID();
		}
		ImGui::EndChild();

		ImGui::SeparatorText("Combat / AI");
		if (ImGui::Button("Kill Hostiles (kah)##npc_kah")) console.Run("player.kah");
		ImGui::SameLine();
		if (ImGui::Button("Kill All (ka)##npc_ka")) console.Run("ka");
		ImGui::SameLine();
		if (ImGui::Button("Toggle AI (tai)##npc_tai")) console.Run("tai");
		ImGui::SameLine();
		if (ImGui::Button("Toggle Detection (tdetect)##npc_tdetect")) console.Run("tdetect");

		ImGui::SeparatorText("Spawn clone (advanced)");
		ImGui::TextDisabled("Creates a duplicate — don't use for Boone/story companions.");
		ImGui::InputText("Base FormID##npc_spawn_id", spawnId_, sizeof(spawnId_));
		ImGui::InputInt("Count##npc_spawn_n", &spawnCount_, 1, 5);
		if (spawnCount_ < 1) spawnCount_ = 1;
		if (spawnCount_ > 20) spawnCount_ = 20;
		if (ImGui::Button("placeatme##npc_place")) {
			console.Runf("player.placeatme %s %d", spawnId_, spawnCount_);
			Notify("Clone spawn sent");
		}

		(void)perf;
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"npc.nearby", "Nearby NPC Scan", "scan heal revive kill bring nearby", FeatureCategory::Npcs, {}});
		out.push_back({"npc.companions", "Companions", "boone veronica cass lily raul arcade rex ed-e", FeatureCategory::Npcs, {}});
		out.push_back({"npc.kill", "Kill Hostiles", "kah ka", FeatureCategory::Npcs, {}});
	}

	nlohmann::json Serialize() const override
	{
		return {{"spawnId", spawnId_}, {"spawnCount", spawnCount_}, {"scanRange", scanRange_}};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		spawnCount_ = j.value("spawnCount", spawnCount_);
		scanRange_ = j.value("scanRange", scanRange_);
		if (j.contains("spawnId") && j["spawnId"].is_string())
			std::snprintf(spawnId_, sizeof(spawnId_), "%s", j["spawnId"].get<std::string>().c_str());
	}

private:
	char spawnId_[16] = "0001A62D";
	int spawnCount_ = 1;
	float scanRange_ = 4000.f;
	bool autoScan_ = true;
	float autoScanAccum_ = 0.f;
	std::vector<NearbyMarker> nearby_;
};

std::unique_ptr<IFeature> CreateNpcFeature()
{
	return std::make_unique<NpcFeature>();
}

} // namespace sfc
