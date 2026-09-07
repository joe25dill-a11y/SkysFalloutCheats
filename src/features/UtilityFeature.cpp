#include "core/Feature.hpp"
#include "core/Config.hpp"
#include "core/Compat.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/Log.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <cstdio>
#include <memory>
#include <string>

namespace sfc {

class UtilityFeature : public IFeature {
public:
	const char* Id() const override { return "utility"; }
	const char* Name() const override { return "Utility"; }
	FeatureCategory Category() const override { return FeatureCategory::Utility; }

	void DrawMenu() override
	{
		ImGui::SeparatorText("Config");
		if (ImGui::Button("Reload Config##util_reload")) {
			const auto& path = Config::Get().Path();
			if (path.empty()) {
				Notify("Config path not set");
			} else if (Config::Get().Load(path)) {
				Notify("Config reloaded");
			} else {
				Notify("Config reload failed");
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Save Config##util_save")) {
			if (Config::Get().Save()) Notify("Config saved");
			else Notify("Config save failed");
		}

		ImGui::SeparatorText("Log");
		const std::string logHint = Compat().pluginDataDir.empty()
			? std::string("(plugin data dir unknown)")
			: (Compat().pluginDataDir + "\\SkysFalloutCheats.log");
		ImGui::TextWrapped("Log path (typical): %s", logHint.c_str());
		ImGui::TextDisabled("Open the log in a text editor while diagnosing issues.");

		ImGui::SeparatorText("Advanced Console");
		if (!ConsoleBridge::Get().IsReady()) {
			ImGui::TextWrapped("Feature unavailable with current framework/version.");
		} else {
			ImGui::InputText("Console line##util_console", consoleLine_, sizeof(consoleLine_));
			if (ImGui::Button("Run##util_run")) {
				if (consoleLine_[0] == '\0') {
					Notify("Enter a console command");
				} else {
					ConsoleBridge::Get().Run(consoleLine_);
					Notify("Console command sent");
					SFC_LOG("Utility console: %s", consoleLine_);
				}
			}
			ImGui::TextDisabled("Runs arbitrary Fallout console text. Use carefully.");
		}
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"utility.reload", "Reload Config", "reload config settings", FeatureCategory::Utility, {}});
		out.push_back({"utility.console", "Run Console Line", "console command advanced", FeatureCategory::Utility, {}});
		out.push_back({"utility.log", "Log Path", "log file debug path", FeatureCategory::Utility, {}});
	}

	nlohmann::json Serialize() const override
	{
		return {{"consoleLine", consoleLine_}};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		if (j.contains("consoleLine") && j["consoleLine"].is_string()) {
			std::snprintf(consoleLine_, sizeof(consoleLine_), "%s", j["consoleLine"].get<std::string>().c_str());
		}
	}

private:
	char consoleLine_[256] = "";
};

std::unique_ptr<IFeature> CreateUtilityFeature()
{
	return std::make_unique<UtilityFeature>();
}

} // namespace sfc
