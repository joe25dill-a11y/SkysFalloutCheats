#include "ui/Notifications.hpp"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace sfc {
namespace {

struct Toast {
	std::string text;
	float remaining = 0.f;
	float duration = 2.5f;
};

std::mutex g_mutex;
std::vector<Toast> g_toasts;

} // namespace

void Notify(const char* text, float durationSec)
{
	if (!text || !*text) return;
	std::lock_guard<std::mutex> lock(g_mutex);
	Toast t;
	t.text = text;
	t.duration = durationSec > 0.1f ? durationSec : 2.5f;
	t.remaining = t.duration;
	g_toasts.push_back(std::move(t));
	if (g_toasts.size() > 8) {
		g_toasts.erase(g_toasts.begin());
	}
}

void DrawNotifications()
{
	const float dt = ImGui::GetIO().DeltaTime;
	std::vector<Toast> local;
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		for (auto& t : g_toasts) {
			t.remaining -= dt;
		}
		g_toasts.erase(
			std::remove_if(g_toasts.begin(), g_toasts.end(),
				[](const Toast& t) { return t.remaining <= 0.f; }),
			g_toasts.end());
		local = g_toasts;
	}

	if (local.empty()) return;

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	const ImVec2 workPos = vp->WorkPos;
	const ImVec2 workSize = vp->WorkSize;
	float y = workPos.y + 16.f;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 8.f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.10f, 0.92f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 0.72f, 0.20f, 0.55f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.93f, 0.88f, 1.f));

	for (size_t i = 0; i < local.size(); ++i) {
		const Toast& t = local[i];
		const float alpha = std::clamp(t.remaining / (t.duration * 0.25f), 0.f, 1.f);
		ImGui::SetNextWindowBgAlpha(0.92f * alpha);
		ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x - 16.f, y), ImGuiCond_Always, ImVec2(1.f, 0.f));
		char winId[64];
		std::snprintf(winId, sizeof(winId), "##sfc_toast_%zu", i);
		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
		if (ImGui::Begin(winId, nullptr, flags)) {
			ImGui::TextUnformatted(t.text.c_str());
			y += ImGui::GetWindowSize().y + 8.f;
		}
		ImGui::End();
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(2);
}

} // namespace sfc
