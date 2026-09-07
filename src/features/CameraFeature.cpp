#include "core/Feature.hpp"
#include "core/ConsoleBridge.hpp"
#include "core/Log.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <memory>

namespace sfc {

class CameraFeature : public IFeature {
public:
	const char* Id() const override { return "camera"; }
	const char* Name() const override { return "Camera"; }
	FeatureCategory Category() const override { return FeatureCategory::Camera; }

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

		ImGui::SeparatorText("Field of View");
		ImGui::SliderFloat("FOV##cam_fov", &fov_, 40.f, 120.f, "%.0f");
		if (ImGui::Button("Apply FOV##cam_apply_fov")) {
			console.Runf("fov %.0f", fov_);
			Notify("FOV applied");
		}

		ImGui::SeparatorText("Free Camera");
		if (ImGui::Checkbox("Free Camera (tfc)##cam_tfc", &tfcOn_)) {
			console.Run("tfc");
			Notify(tfcOn_ ? "Free camera enabled" : "Free camera toggled");
		}
		ImGui::TextWrapped(
			"Free-cam move speed is controlled by the game / TFC bindings. "
			"This plugin does not rewrite camera velocity without deeper SDK hooks.");
		ImGui::SliderFloat("Suggested speed note##cam_speed", &speedNote_, 0.5f, 10.f, "%.1fx");
		ImGui::TextDisabled("Speed slider is informational until camera bindings land.");
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"camera.fov", "FOV", "fov field of view camera", FeatureCategory::Camera, {}});
		out.push_back({"camera.tfc", "Free Camera", "tfc freecam fly camera", FeatureCategory::Camera, {}});
	}

	nlohmann::json Serialize() const override
	{
		return {{"fov", fov_}, {"tfcOn", tfcOn_}, {"speedNote", speedNote_}};
	}

	void Deserialize(const nlohmann::json& j) override
	{
		if (!j.is_object()) return;
		fov_ = j.value("fov", fov_);
		tfcOn_ = j.value("tfcOn", tfcOn_);
		speedNote_ = j.value("speedNote", speedNote_);
	}

private:
	float fov_ = 75.f;
	bool tfcOn_ = false;
	float speedNote_ = 1.f;
};

std::unique_ptr<IFeature> CreateCameraFeature()
{
	return std::make_unique<CameraFeature>();
}

} // namespace sfc
