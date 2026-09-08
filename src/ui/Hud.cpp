#include "ui/Hud.hpp"
#include "ui/Theme.hpp"
#include "core/Config.hpp"
#include "core/Input.hpp"
#include "core/GameState.hpp"
#include "core/Compat.hpp"
#include "imgui.h"
#include <cstdio>
#include <cmath>

namespace sfc {

void Hud::Draw()
{
	auto& hud = Config::Get().Data().hud;
	if (!hud.enabled || !hud.liveHud) return;
	if (Input::Get().MenuOpen() || Input::Get().SearchOpen()) return;

	ImGuiIO& io = ImGui::GetIO();
	ImVec4 accent = Theme::Accent();
	const auto& snap = GameState::Get().Snapshot();

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

	if (hud.statusBar) {
		ImGui::SetNextWindowPos(ImVec2(12.0f + hud.posX, 10.0f + hud.posY), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.58f);
		if (ImGui::Begin("##SFC_Status", nullptr, flags)) {
			ImGui::TextColored(accent, "SFC");
			ImGui::SameLine();
			if (snap.valid && snap.healthStatus == ReadStatus::Valid) {
				ImGui::Text("HP %.0f%%  |  AP %.0f%%", snap.health, snap.ap);
			} else {
				ImGui::Text("INSERT = menu  |  waiting for HUD");
			}
		}
		ImGui::End();
	}

	if (hud.worldInfo) {
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 220.0f, 10.0f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.55f);
		if (ImGui::Begin("##SFC_World", nullptr, flags)) {
			ImGui::TextColored(accent, "WORLD");
			ImGui::SameLine();
			if (!snap.location.empty())
				ImGui::Text("%s", snap.location.c_str());
			else
				ImGui::TextDisabled("(explore for label)");
		}
		ImGui::End();
	}

	if (hud.combatInfo) {
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - 120.0f, io.DisplaySize.y - 42.0f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.50f);
		if (ImGui::Begin("##SFC_Combat", nullptr, flags)) {
			ImGui::TextColored(accent, "AMMO");
			ImGui::SameLine();
			if (snap.weaponStatus == ReadStatus::Valid && snap.ammoClip >= 0)
				ImGui::Text("%d / %d", snap.ammoClip, snap.ammoReserve >= 0 ? snap.ammoReserve : 0);
			else if (snap.weaponStatus == ReadStatus::Valid && !snap.weaponName.empty())
				ImGui::Text("%s", snap.weaponName.c_str());
			else
				ImGui::TextDisabled("--");
		}
		ImGui::End();
	}

	if (hud.fps) {
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 90.0f, io.DisplaySize.y - 36.0f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.40f);
		if (ImGui::Begin("##SFC_FPS", nullptr, flags)) {
			ImGui::Text("FPS %.0f", io.Framerate);
		}
		ImGui::End();
	}
}

} // namespace sfc
