#include "ui/Hud.hpp"
#include "ui/Theme.hpp"
#include "core/Config.hpp"
#include "core/Input.hpp"
#include "core/GameState.hpp"
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

	// Full SFC live HUD back — top-left (Fallout HP/AP sit bottom; we stay out of that band).
	if (hud.statusBar) {
		ImGui::SetNextWindowPos(ImVec2(12.0f + hud.posX, 10.0f + hud.posY), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.55f);
		if (ImGui::Begin("##SFC_Status", nullptr, flags)) {
			ImGui::TextColored(accent, "SFC");
			ImGui::SameLine();
			if (snap.valid && snap.healthStatus == ReadStatus::Valid) {
				const char* loc = snap.location.empty() ? "--" : snap.location.c_str();
				if (snap.capsStatus == ReadStatus::Valid && snap.caps >= 0) {
					ImGui::Text("LVL %d  |  HP %.0f/%.0f  |  AP %.0f/%.0f  |  CAPS %d  |  %s",
						snap.level, snap.health, snap.healthMax, snap.ap, snap.apMax, snap.caps, loc);
				} else {
					ImGui::Text("LVL %d  |  HP %.0f/%.0f  |  AP %.0f/%.0f  |  CAPS --  |  %s",
						snap.level, snap.health, snap.healthMax, snap.ap, snap.apMax, loc);
				}
			} else {
				ImGui::Text("LVL --  |  HP --/--  |  AP --/--  |  CAPS --  |  --");
			}
		}
		ImGui::End();
	}

	if (hud.worldInfo) {
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 200.0f, 10.0f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.55f);
		if (ImGui::Begin("##SFC_World", nullptr, flags)) {
			ImGui::TextColored(accent, "WORLD");
			ImGui::SameLine();
			if (snap.valid && snap.gameHour >= 0.f) {
				const int hour = static_cast<int>(snap.gameHour) % 24;
				const int mins = static_cast<int>((snap.gameHour - static_cast<int>(snap.gameHour)) * 60.f) % 60;
				ImGui::Text("TIME %02d:%02d", hour, mins);
			} else {
				ImGui::Text("TIME --:--");
			}
			if (snap.hasPos && std::isfinite(snap.posX) && std::isfinite(snap.posY) && std::isfinite(snap.posZ)) {
				ImGui::Text("XYZ %.0f %.0f %.0f", snap.posX, snap.posY, snap.posZ);
			}
		}
		ImGui::End();
	}

	if (hud.combatInfo) {
		// Bottom-center — avoid Fallout's bottom-left HP and bottom-right ammo corners.
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - 160.0f, io.DisplaySize.y - 42.0f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.50f);
		if (ImGui::Begin("##SFC_Combat", nullptr, flags)) {
			ImGui::TextColored(accent, "WPN");
			ImGui::SameLine();
			const char* wpn = (snap.weaponStatus == ReadStatus::Valid && !snap.weaponName.empty())
				? snap.weaponName.c_str() : "--";
			if (snap.weaponStatus == ReadStatus::Valid && snap.ammoClip >= 0 && snap.ammoClipMax >= 0) {
				if (snap.ammoReserve >= 0)
					ImGui::Text("%s  |  AMMO %d/%d (%d)", wpn, snap.ammoClip, snap.ammoClipMax, snap.ammoReserve);
				else
					ImGui::Text("%s  |  AMMO %d/%d", wpn, snap.ammoClip, snap.ammoClipMax);
			} else {
				ImGui::Text("%s  |  AMMO --", wpn);
			}
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
