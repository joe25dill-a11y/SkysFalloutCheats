#include "core/Feature.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/CompanionFollow.hpp"
#include "core/Input.hpp"
#include "core/Log.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <cstdint>
#include <cstdio>
#include <memory>

namespace sfc {
namespace {

struct Companion {
	const char* name;
	const char* baseId; // for setessential
	std::uint32_t refId; // original world instance — moveto, not placeatme
};

// Vanilla FNV companions: base + ref FormIDs (FalloutNV.esm / wiki).
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

		ImGui::SeparatorText("Companions");
		ImGui::TextWrapped(
			"Bring uses moveto on the original companion (same idea as Pip-Boy / door travel). "
			"SFC teleports auto-bring active teammates when enabled on the Teleport tab.");
		if (ImGui::Button("Bring all teammates here##npc_bring_all")) {
			const int n = CompanionFollow::Get().BringTeammatesNow();
			if (n > 0) Notify("Teammates moving to you");
			else Notify("No teammates in process lists — try Bring on a name below");
		}
		ImGui::SameLine();
		{
			bool follow = CompanionFollow::Get().Enabled();
			if (ImGui::Checkbox("Auto-follow SFC teleports##npc_auto", &follow)) {
				CompanionFollow::Get().SetEnabled(follow);
			}
		}

		ImGui::BeginChild("##npc_companions", ImVec2(0, 220), true);
		for (const auto& c : kCompanions) {
			ImGui::PushID(c.baseId);
			ImGui::TextUnformatted(c.name);
			ImGui::SameLine(200.f);
			ImGui::TextDisabled("%08X", c.refId);
			ImGui::SameLine(290.f);
			if (ImGui::SmallButton("Bring##npc_bring")) {
				CompanionFollow::Get().BringRef(c.refId);
				Notify(c.name);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Essential##npc_ess")) {
				console.Runf("setessential %s 1", c.baseId);
				std::snprintf(essentialId_, sizeof(essentialId_), "%s", c.baseId);
				Notify("setessential 1");
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Use ID##npc_use")) {
				std::snprintf(essentialId_, sizeof(essentialId_), "%s", c.baseId);
				Notify("FormID filled");
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
		ImGui::TextDisabled("Clone spawn (placeatme) is under Spawn custom — avoid for companions.");

		ImGui::SeparatorText("Combat / AI");
		if (ImGui::Button("Kill Hostiles (kah)##npc_kah")) {
			console.Run("player.kah");
			Notify("Kill all hostiles");
		}
		ImGui::SameLine();
		if (ImGui::Button("Kill All (ka)##npc_ka")) {
			console.Run("ka");
			Notify("Kill all");
		}
		ImGui::SameLine();
		if (ImGui::Button("Kill (selected)##npc_kill")) {
			console.Run("kill");
			Notify("Kill command sent");
		}

		if (ImGui::Button("Toggle AI (tai)##npc_tai")) {
			console.Run("tai");
			Notify("tai");
		}
		ImGui::SameLine();
		if (ImGui::Button("Toggle Combat AI (tcai)##npc_tcai")) {
			console.Run("tcai");
			Notify("tcai");
		}
		ImGui::SameLine();
		if (ImGui::Button("Toggle Detection (tdetect)##npc_tdetect")) {
			console.Run("tdetect");
			Notify("tdetect");
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset AI (selected)##npc_resetai")) {
			console.Run("resetai");
			Notify("resetai");
		}

		ImGui::SeparatorText("Selected Target");
		ImGui::TextWrapped("Console-select an NPC in the world, or face a companion.");
		if (ImGui::Button("Resurrect##npc_resurrect")) {
			Input::Get().SetMenuOpen(false);
			console.Run("resurrect 1");
			Notify("Resurrect queued (menu closed)");
		}
		ImGui::SameLine();
		if (ImGui::Button("Resurrect Player##npc_resurrect_player")) {
			Input::Get().SetMenuOpen(false);
			console.Run("player.resurrect 1");
			Notify("Player resurrect queued — wait a moment");
		}
		ImGui::SameLine();
		if (ImGui::Button("Open Inventory##npc_openinv")) {
			console.Run("OpenTeammateContainer 1");
			Notify("OpenTeammateContainer");
		}
		if (ImGui::Button("Wait / Follow tip##npc_wait_tip")) {
			Notify("Use companion wheel in-game for Wait/Follow");
		}
		ImGui::SameLine();
		if (ImGui::Button("Get AV Health (selected)##npc_getav")) {
			console.Run("getav health");
			Notify("getav health (see console)");
		}

		ImGui::SliderFloat("Scale##npc_scale", &scale_, 0.1f, 5.f, "%.2f");
		if (ImGui::Button("Set Scale (selected)##npc_setscale")) {
			console.Runf("setscale %.2f", scale_);
			Notify("setscale");
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Scale 1.0##npc_resetscale")) {
			scale_ = 1.f;
			console.Run("setscale 1");
		}

		ImGui::SeparatorText("Essential (custom)");
		ImGui::InputText("Base FormID##npc_essential_id", essentialId_, sizeof(essentialId_));
		if (ImGui::Button("Set Essential##npc_set_essential")) {
			console.Runf("setessential %s 1", essentialId_);
			Notify("setessential 1");
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Essential##npc_clear_essential")) {
			console.Runf("setessential %s 0", essentialId_);
			Notify("setessential 0");
		}

		ImGui::SeparatorText("Spawn custom (clones)");
		ImGui::InputText("Base FormID##npc_spawn_id", spawnId_, sizeof(spawnId_));
		ImGui::InputInt("Count##npc_spawn_n", &spawnCount_, 1, 5);
		if (spawnCount_ < 1) spawnCount_ = 1;
		if (spawnCount_ > 20) spawnCount_ = 20;
		if (ImGui::Button("player.placeatme##npc_place")) {
			console.Runf("player.placeatme %s %d", spawnId_, spawnCount_);
			Notify("placeatme sent");
		}
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"npc.companions", "Companions", "boone veronica cass lily raul arcade rex ed-e bring moveto", FeatureCategory::Npcs, {}});
		out.push_back({"npc.kill", "Kill Target / Hostiles", "kill kah npc hostile", FeatureCategory::Npcs, {}});
		out.push_back({"npc.resurrect", "Resurrect", "resurrect revive npc", FeatureCategory::Npcs, {}});
		out.push_back({"npc.essential", "Set Essential", "essential immortal npc", FeatureCategory::Npcs, {}});
		out.push_back({"npc.ai", "Toggle AI / Detection", "tai tcai tdetect resetai", FeatureCategory::Npcs, {}});
	}

	nlohmann::json Serialize() const override
	{
		return {{"essentialId", essentialId_}, {"spawnId", spawnId_}, {"scale", scale_}, {"spawnCount", spawnCount_}};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		scale_ = j.value("scale", scale_);
		spawnCount_ = j.value("spawnCount", spawnCount_);
		if (j.contains("essentialId") && j["essentialId"].is_string()) {
			std::snprintf(essentialId_, sizeof(essentialId_), "%s", j["essentialId"].get<std::string>().c_str());
		}
		if (j.contains("spawnId") && j["spawnId"].is_string()) {
			std::snprintf(spawnId_, sizeof(spawnId_), "%s", j["spawnId"].get<std::string>().c_str());
		}
	}

private:
	char essentialId_[16] = "00092BD2";
	char spawnId_[16] = "00092BD2";
	int spawnCount_ = 1;
	float scale_ = 1.f;
};

std::unique_ptr<IFeature> CreateNpcFeature()
{
	return std::make_unique<NpcFeature>();
}

} // namespace sfc
