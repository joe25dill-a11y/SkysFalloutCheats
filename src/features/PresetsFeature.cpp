#include "core/Feature.hpp"
#include "core/FeatureRegistry.hpp"
#include "core/Config.hpp"
#include "core/Compat.hpp"
#include "core/LootVacuum.hpp"
#include "core/Log.hpp"
#include "ui/Notifications.hpp"
#include "imgui.h"
#include <nlohmann/json.hpp>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sfc {
namespace {

const char* kBuiltinNames[] = {
	"Exploration", "Combat", "Loot", "Testing", "Screenshot",
	"Immersion", "GodMode", "MinimalHUD", "AimMagic", "AimCombat"
};

std::filesystem::path PresetsDir()
{
	return std::filesystem::path(Compat().pluginDataDir) / "presets";
}

std::string SanitizeFileStem(const std::string& name)
{
	std::string out;
	for (char c : name) {
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '_' || c == '-') {
			out.push_back(c);
		} else if (c == ' ') {
			out.push_back('_');
		}
	}
	if (out.empty()) out = "preset";
	return out;
}

nlohmann::json CapturePreset()
{
	nlohmann::json j;
	j["schemaVersion"] = 1;
	j["config"] = ToJson(Config::Get().Data());
	j["features"] = nlohmann::json::object();
	for (const auto& f : FeatureRegistry::Get().All()) {
		j["features"][f->Id()] = f->Serialize();
	}
	return j;
}

void ApplyPreset(const nlohmann::json& j)
{
	if (j.contains("config")) {
		FromJson(j["config"], Config::Get().Data());
		Config::Get().MarkDirty();
	}
	if (j.contains("features") && j["features"].is_object()) {
		for (auto it = j["features"].begin(); it != j["features"].end(); ++it) {
			if (IFeature* f = FeatureRegistry::Get().Find(it.key().c_str())) {
				f->Deserialize(it.value());
			}
		}
	}
}

} // namespace

class PresetsFeature : public IFeature {
public:
	const char* Id() const override { return "presets"; }
	const char* Name() const override { return "Presets"; }
	FeatureCategory Category() const override { return FeatureCategory::Presets; }

	bool Init() override
	{
		RefreshList();
		return true;
	}

	void DrawMenu() override
	{
		ImGui::SeparatorText("Built-in");
		for (const char* name : kBuiltinNames) {
			ImGui::PushID(name);
			ImGui::TextUnformatted(name);
			ImGui::SameLine(180.f);
			if (ImGui::SmallButton("Save##builtin")) {
				SaveNamed(name);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Load##builtin")) {
				LoadNamed(name);
			}
			ImGui::PopID();
		}

		ImGui::SeparatorText("Custom");
		ImGui::InputText("Preset name##preset_name", nameBuf_, sizeof(nameBuf_));
		if (ImGui::Button("Save New##preset_save")) {
			if (nameBuf_[0] == '\0') Notify("Enter a preset name");
			else SaveNamed(nameBuf_);
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh List##preset_refresh")) {
			RefreshList();
		}

		ImGui::Spacing();
		for (size_t i = 0; i < files_.size(); ++i) {
			ImGui::PushID(static_cast<int>(i));
			ImGui::TextUnformatted(files_[i].c_str());
			ImGui::SameLine(220.f);
			if (ImGui::SmallButton("Load##p")) {
				LoadNamed(files_[i]);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Dup##p")) {
				DuplicateNamed(files_[i]);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Del##p")) {
				DeleteNamed(files_[i]);
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}
		if (files_.empty()) {
			ImGui::TextDisabled("No preset files in pluginDataDir/presets/");
		}
	}

	void CollectSearch(std::vector<SearchEntry>& out) override
	{
		out.push_back({"presets.save", "Save Preset", "preset save profile", FeatureCategory::Presets, {}});
		out.push_back({"presets.load", "Load Preset", "preset load profile exploration combat", FeatureCategory::Presets, {}});
	}

private:
	void RefreshList()
	{
		files_.clear();
		const auto dir = PresetsDir();
		std::error_code ec;
		if (!std::filesystem::exists(dir, ec)) return;
		for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
			if (!entry.is_regular_file()) continue;
			if (entry.path().extension() == ".json") {
				files_.push_back(entry.path().stem().string());
			}
		}
	}

	std::filesystem::path PathFor(const std::string& name) const
	{
		return PresetsDir() / (SanitizeFileStem(name) + ".json");
	}

	void SaveNamed(const std::string& name)
	{
		const auto path = PathFor(name);
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
		nlohmann::json j = CapturePreset();
		j["name"] = name;
		std::ofstream out(path);
		if (!out) {
			Notify("Failed to save preset");
			return;
		}
		out << j.dump(2);
		Config::Get().Data().lastPreset = name;
		Config::Get().MarkDirty();
		RefreshList();
		Notify("Preset saved");
		SFC_LOG("Preset saved: %s", path.string().c_str());
	}

	void LoadNamed(const std::string& name)
	{
		const auto path = PathFor(name);
		std::ifstream in(path);
		if (!in) {
			if (_stricmp(name.c_str(), "Loot") == 0) {
				LootVacuum::Get().ApplyLootPresetDefaults();
				Config::Get().Data().performance.espEnabled = true;
				if (IFeature* esp = FeatureRegistry::Get().Find("esp")) {
					nlohmann::json merge = esp->Serialize();
					auto loot = LootVacuum::Get().Serialize();
					for (auto it = loot.begin(); it != loot.end(); ++it)
						merge[it.key()] = it.value();
					merge["enabled"] = true;
					merge["showLoot"] = true;
					merge["showContainers"] = true;
					esp->Deserialize(merge);
				}
				Config::Get().Data().lastPreset = name;
				Config::Get().MarkDirty();
				Notify("Loot preset applied");
				return;
			}
			Notify("Preset not found");
			return;
		}
		try {
			nlohmann::json j;
			in >> j;
			ApplyPreset(j);
			if (_stricmp(name.c_str(), "Loot") == 0 && j.contains("features") && j["features"].contains("esp"))
				LootVacuum::Get().Deserialize(j["features"]["esp"]);
			Config::Get().Data().lastPreset = name;
			Config::Get().MarkDirty();
			Notify("Preset loaded");
		} catch (const std::exception& ex) {
			SFC_WARN("Preset load failed: %s", ex.what());
			Notify("Preset load failed");
		}
	}

	void DuplicateNamed(const std::string& name)
	{
		const auto src = PathFor(name);
		std::ifstream in(src);
		if (!in) {
			Notify("Source missing");
			return;
		}
		std::string copyName = name + "_copy";
		std::ofstream out(PathFor(copyName));
		if (!out) {
			Notify("Duplicate failed");
			return;
		}
		out << in.rdbuf();
		RefreshList();
		Notify("Preset duplicated");
	}

	void DeleteNamed(const std::string& name)
	{
		std::error_code ec;
		std::filesystem::remove(PathFor(name), ec);
		RefreshList();
		Notify(ec ? "Delete failed" : "Preset deleted");
	}

	char nameBuf_[64] = "";
	std::vector<std::string> files_;
};

std::unique_ptr<IFeature> CreatePresetsFeature()
{
	return std::make_unique<PresetsFeature>();
}

} // namespace sfc
