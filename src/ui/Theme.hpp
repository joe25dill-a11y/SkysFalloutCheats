#pragma once
#include "imgui.h"
#include "core/Config.hpp"

namespace sfc {

class Theme {
public:
	static void Apply(const ThemeSettings& s);
	static ImVec4 Accent();
	static void DrawCrtOverlay(bool scanlines, bool glow);
};

} // namespace sfc
