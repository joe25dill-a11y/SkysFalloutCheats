#include "core/Feature.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/Log.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <cstdio>
#include <memory>
#include <string>

namespace sfc {

class QuestFeature : public IFeature {
public:
	const char* Id() const override { return "quest"; }
	const char* Name() const override { return "Quests"; }
	FeatureCategory Category() const override { return FeatureCategory::Quests; }

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

		ImGui::SeparatorText("Info");
		ImGui::TextWrapped(
			"Quest helpers use console commands only. Incorrect FormIDs or stages can soft-lock "
			"storylines. Prefer in-game progression unless you know the exact stage table.");

		ImGui::SeparatorText("DANGER ZONE — setstage");
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.45f, 0.35f, 1.f));
		ImGui::TextUnformatted("Can break quests permanently for this save.");
		ImGui::PopStyleColor();
		ImGui::InputText("Quest FormID (hex)##quest_id", questId_, sizeof(questId_));
		ImGui::InputInt("Stage##quest_stage", &stage_, 1, 10);
		if (stage_ < 0) stage_ = 0;
		ImGui::Checkbox("I accept the risk##quest_confirm", &confirm_);
		ImGui::BeginDisabled(!confirm_);
		if (ImGui::Button("Run setstage##quest_setstage")) {
			std::string id = SanitizeFormId(questId_);
			if (id.empty()) {
				Notify("Invalid quest FormID");
			} else {
				console.Runf("setstage %s %d", id.c_str(), stage_);
				Notify("setstage executed");
			}
		}
		ImGui::EndDisabled();

		ImGui::SeparatorText("Helpers");
		if (ImGui::Button("sqt (show quest targets)##quest_sqt")) {
			console.Run("sqt");
			Notify("sqt sent to console");
		}
		ImGui::SameLine();
		if (ImGui::Button("Complete Quest##quest_complete")) {
			if (!confirm_) {
				Notify("Enable danger confirmation first");
			} else {
				std::string id = SanitizeFormId(questId_);
				if (!id.empty()) {
					console.Runf("completequest %s", id.c_str());
					Notify("completequest sent");
				}
			}
		}

		ImGui::SeparatorText("Common Quests (fill FormID)");
		struct Q { const char* name; const char* id; };
		static const Q kQuests[] = {
			{"Ain't That a Kick in the Head", "00104C13"},
			{"Back in the Saddle", "00104C17"},
			{"By a Campfire on the Trail", "00104C19"},
			{"They Went That-A-Way", "00104C14"},
			{"Ring-a-Ding-Ding!", "0010E1DE"},
			{"The House Always Wins", "0011E4F1"},
			{"Render Unto Caesar", "0011E4F2"},
			{"For the Republic", "0011E4F3"},
			{"Wild Card", "0011E4F4"},
			{"Come Fly With Me", "00104C1A"},
			{"Ghost Town Gunfight", "00104C15"},
			{"Run Goodsprings Run", "00104C16"},
		};
		for (const auto& q : kQuests) {
			ImGui::PushID(q.id);
			ImGui::TextUnformatted(q.name);
			ImGui::SameLine(280.f);
			ImGui::TextDisabled("%s", q.id);
			ImGui::SameLine(380.f);
			if (ImGui::SmallButton("Use##quest_pick")) {
				std::snprintf(questId_, sizeof(questId_), "%s", q.id);
				Notify(q.name);
			}
			ImGui::PopID();
		}
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"quest.setstage", "Set Quest Stage", "setstage quest stage danger", FeatureCategory::Quests, {}});
		out.push_back({"quest.sqt", "Show Quest Targets", "sqt objectives quest", FeatureCategory::Quests, {}});
	}

	nlohmann::json Serialize() const override
	{
		return {{"questId", questId_}, {"stage", stage_}};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		stage_ = j.value("stage", stage_);
		if (j.contains("questId") && j["questId"].is_string()) {
			std::snprintf(questId_, sizeof(questId_), "%s", j["questId"].get<std::string>().c_str());
		}
	}

private:
	static std::string SanitizeFormId(const char* raw)
	{
		if (!raw) return {};
		std::string out;
		for (const char* p = raw; *p; ++p) {
			const char c = *p;
			if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
				out.push_back(c);
			}
		}
		if (out.empty() || out.size() > 8) return {};
		while (out.size() < 8) out.insert(out.begin(), '0');
		return out;
	}

	char questId_[16] = "00000000";
	int stage_ = 0;
	bool confirm_ = false;
};

std::unique_ptr<IFeature> CreateQuestFeature()
{
	return std::make_unique<QuestFeature>();
}

} // namespace sfc
