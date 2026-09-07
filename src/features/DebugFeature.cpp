#include "core/Feature.hpp"
#include "core/FeatureRegistry.hpp"
#include "core/Compat.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/Config.hpp"
#include "core/Log.hpp"
#include "imgui.h"
#include <cstdio>
#include <memory>

namespace sfc {

class DebugFeature : public IFeature {
public:
	const char* Id() const override { return "debug"; }
	const char* Name() const override { return "Debug"; }
	FeatureCategory Category() const override { return FeatureCategory::Debug; }

	void DrawMenu() override
	{
		const auto& c = Compat();

		ImGui::SeparatorText("Compat State");
		ImGui::Text("NVSE OK: %s", c.nvseOk ? "yes" : "no");
		ImGui::Text("Console OK: %s", c.consoleOk ? "yes" : "no");
		ImGui::Text("ConsoleBridge ready: %s", ConsoleBridge::Get().IsReady() ? "yes" : "no");
		ImGui::Text("JIP present: %s", c.jipPresent ? "yes" : "no");
		ImGui::Text("JohnnyGuitar present: %s", c.johnnyPresent ? "yes" : "no");
		ImGui::Text("NVSE version: %u", c.nvseVersion);
		ImGui::Text("Runtime version: %u", c.runtimeVersion);
		ImGui::TextWrapped("Runtime dir: %s", c.runtimeDir.c_str());
		ImGui::TextWrapped("Plugin data: %s", c.pluginDataDir.c_str());
		ImGui::TextWrapped("Config path: %s", Config::Get().Path().string().c_str());

		ImGui::SeparatorText("Performance Placeholder");
		const float fps = ImGui::GetIO().Framerate;
		ImGui::Text("ImGui framerate: %.1f FPS", fps);
		ImGui::TextDisabled("Game FPS counter wires in with the D3D present hook.");

		ImGui::SeparatorText("Features");
		for (const auto& f : FeatureRegistry::Get().All()) {
			ImGui::PushID(f->Id());
			const bool ok = f->IsAvailable();
			ImGui::Text("%s [%s] — %s", f->Name(), f->Id(), ok ? "available" : "unavailable");
			if (!ok) {
				ImGui::SameLine();
				ImGui::TextDisabled("(%s)", f->UnavailableReason());
			}
			ImGui::PopID();
		}
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"debug.compat", "Compat State", "debug nvse jip console compat", FeatureCategory::Debug, {}});
		out.push_back({"debug.features", "Feature Availability", "debug features list status", FeatureCategory::Debug, {}});
		out.push_back({"debug.fps", "FPS Placeholder", "fps framerate performance", FeatureCategory::Debug, {}});
	}
};

std::unique_ptr<IFeature> CreateDebugFeature()
{
	return std::make_unique<DebugFeature>();
}

} // namespace sfc
