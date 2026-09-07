#include "ui/SearchOverlay.hpp"
#include "ui/Theme.hpp"
#include "core/Input.hpp"
#include "core/SearchIndex.hpp"
#include "imgui.h"
#include <cstring>

namespace sfc {
namespace {
char g_query[128]{};
int g_focus = 0;
}

void SearchOverlay::Draw()
{
	if (!Input::Get().SearchOpen()) return;

	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowSize(ImVec2(520, 360), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - 260.0f, io.DisplaySize.y * 0.18f), ImGuiCond_Always);

	if (!ImGui::Begin("##SFC_Search", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
		ImGui::End();
		return;
	}

	ImGui::TextColored(Theme::Accent(), "COMMAND SEARCH");
	ImGui::SetKeyboardFocusHere(g_focus == 0 ? 0 : -1);
	if (ImGui::InputTextWithHint("##q", "Search features (god, tele, ammo...)", g_query, sizeof(g_query))) {
		g_focus = 1;
	}

	auto results = SearchIndex::Get().Query(g_query, 16);
	ImGui::BeginChild("##results", ImVec2(0, 0), true);
	for (auto& r : results) {
		ImGui::TextColored(Theme::Accent(), "%s", CategoryName(r.category));
		ImGui::SameLine();
		if (ImGui::Selectable(r.title.c_str())) {
			if (r.activate) r.activate();
			else {
				Input::Get().SetSearchOpen(false);
				Input::Get().SetMenuOpen(true);
			}
		}
		if (!r.keywords.empty()) {
			ImGui::SameLine();
			ImGui::TextDisabled("%s", r.keywords.c_str());
		}
	}
	if (results.empty()) ImGui::TextDisabled("No matches.");
	ImGui::EndChild();
	ImGui::End();
}

} // namespace sfc
