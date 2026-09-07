#include "ui/Theme.hpp"
#include "imgui.h"
#include <cmath>

namespace sfc {
namespace {
ThemeSettings g_theme;
}

void Theme::Apply(const ThemeSettings& s)
{
	g_theme = s;
	if (!ImGui::GetCurrentContext()) return;
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 4.0f;
	style.FrameRounding = 3.0f;
	style.GrabRounding = 2.0f;
	style.ScrollbarRounding = 3.0f;
	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.WindowPadding = ImVec2(14, 12);
	style.FramePadding = ImVec2(10, 6);
	style.ItemSpacing = ImVec2(10, 8);
	style.IndentSpacing = 16.0f;
	style.Alpha = s.opacity;

	ImVec4 accent(s.accentR, s.accentG, s.accentB, 1.0f);
	ImVec4 bg(0.05f, 0.055f, 0.05f, s.opacity);
	ImVec4 panel(0.08f, 0.09f, 0.075f, s.opacity);
	ImVec4 text(0.92f, 0.90f, 0.78f, 1.0f);
	ImVec4 muted(0.55f, 0.52f, 0.40f, 1.0f);

	ImVec4* c = style.Colors;
	c[ImGuiCol_Text] = text;
	c[ImGuiCol_TextDisabled] = muted;
	c[ImGuiCol_WindowBg] = bg;
	c[ImGuiCol_ChildBg] = panel;
	c[ImGuiCol_PopupBg] = bg;
	c[ImGuiCol_Border] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
	c[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.10f, 0.9f);
	c[ImGuiCol_FrameBgHovered] = ImVec4(accent.x, accent.y, accent.z, 0.25f);
	c[ImGuiCol_FrameBgActive] = ImVec4(accent.x, accent.y, accent.z, 0.40f);
	c[ImGuiCol_TitleBg] = panel;
	c[ImGuiCol_TitleBgActive] = panel;
	c[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.22f);
	c[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
	c[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.45f);
	c[ImGuiCol_Button] = ImVec4(accent.x, accent.y, accent.z, 0.20f);
	c[ImGuiCol_ButtonHovered] = ImVec4(accent.x, accent.y, accent.z, 0.40f);
	c[ImGuiCol_ButtonActive] = ImVec4(accent.x, accent.y, accent.z, 0.55f);
	c[ImGuiCol_CheckMark] = accent;
	c[ImGuiCol_SliderGrab] = accent;
	c[ImGuiCol_SliderGrabActive] = ImVec4(1, 0.85f, 0.4f, 1);
	c[ImGuiCol_Separator] = ImVec4(accent.x, accent.y, accent.z, 0.25f);
	c[ImGuiCol_Tab] = panel;
	c[ImGuiCol_TabHovered] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
	c[ImGuiCol_TabSelected] = ImVec4(accent.x, accent.y, accent.z, 0.30f);
	c[ImGuiCol_ScrollbarGrab] = ImVec4(accent.x, accent.y, accent.z, 0.35f);

	ImGui::GetIO().FontGlobalScale = (s.uiScale > 0.1f) ? s.uiScale : 1.0f;
	ImGui::GetIO().DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
}

ImVec4 Theme::Accent()
{
	return ImVec4(g_theme.accentR, g_theme.accentG, g_theme.accentB, 1.0f);
}

void Theme::DrawCrtOverlay(bool scanlines, bool glow)
{
	if (!scanlines && !glow) return;
	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	const ImVec2 ds = ImGui::GetIO().DisplaySize;
	if (glow) {
		ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(g_theme.accentR, g_theme.accentG, g_theme.accentB, 0.035f));
		dl->AddRectFilled(ImVec2(0, 0), ds, col);
	}
	if (scanlines) {
		ImU32 line = IM_COL32(0, 0, 0, 28);
		for (float y = 0; y < ds.y; y += 3.0f)
			dl->AddLine(ImVec2(0, y), ImVec2(ds.x, y), line);
	}
}

} // namespace sfc
