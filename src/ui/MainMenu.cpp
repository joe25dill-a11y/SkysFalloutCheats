#include "ui/MainMenu.hpp"
#include "ui/Theme.hpp"
#include "core/Input.hpp"
#include "core/FeatureRegistry.hpp"
#include "core/Config.hpp"
#include "core/Compat.hpp"
#include "core/GameState.hpp"
#include "imgui.h"
#include <string>
#include <cctype>

namespace sfc {
namespace {

struct NavItem {
	const char* label;
	FeatureCategory category;
};

const NavItem kNav[] = {
	{"PLAYER", FeatureCategory::Player},
	{"WEAPONS", FeatureCategory::Weapons},
	{"INVENTORY", FeatureCategory::Inventory},
	{"SKILLS", FeatureCategory::Skills},
	{"NPCs", FeatureCategory::Npcs},
	{"WORLD", FeatureCategory::World},
	{"TELEPORT", FeatureCategory::Teleport},
	{"QUESTS", FeatureCategory::Quests},
	{"CAMERA", FeatureCategory::Camera},
	{"ESP", FeatureCategory::Esp},
	{"PRESETS", FeatureCategory::Presets},
	{"UTILITY", FeatureCategory::Utility},
	{"SETTINGS", FeatureCategory::Settings},
	{"DEBUG", FeatureCategory::Debug},
};

int g_selected = 0;
char g_filter[64]{};
}

void MainMenu::Draw()
{
	if (!Input::Get().MenuOpen()) return;
	// No CRT fullscreen overlays — those are a common source of “broken look”.

	ImGuiIO& io = ImGui::GetIO();
	const ImVec2 size(io.DisplaySize.x * 0.72f, io.DisplaySize.y * 0.78f);
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.14f, io.DisplaySize.y * 0.11f), ImGuiCond_FirstUseEver);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (!ImGui::Begin("##SFC_Main", nullptr, flags)) {
		ImGui::End();
		return;
	}

	ImVec4 accent = Theme::Accent();
	ImGui::TextColored(accent, "SKY'S FALLOUT CHEATS");
	ImGui::SameLine();
	ImGui::TextDisabled("  Utility Console // playable-v17g");
	{
		const auto& snap = GameState::Get().Snapshot();
		if (snap.valid && snap.healthStatus == ReadStatus::Valid) {
			if (snap.ammoClip >= 0)
				ImGui::Text("HP %.0f%%   AP %.0f%%   AMMO %d/%d   %s",
					snap.health, snap.ap, snap.ammoClip, snap.ammoReserve,
					snap.location.empty() ? "" : snap.location.c_str());
			else
				ImGui::Text("HP %.0f%%   AP %.0f%%   %s",
					snap.health, snap.ap,
					snap.location.empty() ? "" : snap.location.c_str());
		} else {
			ImGui::TextDisabled("Live HUD syncing from vanilla meters...");
		}
		ImGui::TextDisabled("INSERT menu  |  ESC close  |  F1 search  |  JIP=%s",
			Compat().jipPresent ? "yes" : "no");
	}
	ImGui::Separator();

	ImGui::BeginChild("##nav", ImVec2(180, 0), true);
	for (int i = 0; i < static_cast<int>(sizeof(kNav) / sizeof(kNav[0])); ++i) {
		const bool sel = (g_selected == i);
		if (sel) {
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(accent.x, accent.y, accent.z, 0.35f));
			ImGui::PushStyleColor(ImGuiCol_Text, accent);
		}
		if (ImGui::Selectable(kNav[i].label, sel)) g_selected = i;
		if (sel) ImGui::PopStyleColor(2);
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("##content", ImVec2(0, 0), true);
	ImGui::InputTextWithHint("##filter", "Filter section features...", g_filter, sizeof(g_filter));
	ImGui::Spacing();

	auto features = FeatureRegistry::Get().ByCategory(kNav[g_selected].category);
	if (features.empty()) {
		ImGui::TextDisabled("No modules in this section yet.");
	}
	for (auto* f : features) {
		if (g_filter[0]) {
			std::string hay = std::string(f->Name()) + f->Id();
			std::string needle = g_filter;
			for (auto& c : hay) c = (char)tolower((unsigned char)c);
			for (auto& c : needle) c = (char)tolower((unsigned char)c);
			if (hay.find(needle) == std::string::npos) continue;
		}
		if (ImGui::CollapsingHeader(f->Name(), ImGuiTreeNodeFlags_DefaultOpen)) {
			if (!f->IsAvailable()) {
				ImGui::TextColored(ImVec4(1, 0.45f, 0.35f, 1), "%s", f->UnavailableReason());
			} else {
				f->DrawMenu();
			}
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

} // namespace sfc
