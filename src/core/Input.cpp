#include "core/Input.hpp"
#include "core/Config.hpp"

namespace sfc {

Input& Input::Get()
{
	static Input instance;
	return instance;
}

void Input::Tick()
{
	for (int i = 0; i < 256; ++i) {
		const bool down = (GetAsyncKeyState(i) & 0x8000) != 0;
		pressed_[i] = down && !prev_[i];
		prev_[i] = down;
	}

	auto& cfg = Config::Get().Data().controls;
	if (ConsumePressed(cfg.menuToggleVk)) ToggleMenu();
	if (ConsumePressed(cfg.searchToggleVk)) ToggleSearch();
	if (menuOpen_ && ConsumePressed(VK_ESCAPE)) SetMenuOpen(false);
	if (searchOpen_ && ConsumePressed(VK_ESCAPE)) SetSearchOpen(false);
}

bool Input::KeyPressed(int vk) const
{
	if (vk < 0 || vk > 255) return false;
	return pressed_[vk];
}

bool Input::KeyDown(int vk) const
{
	if (vk < 0 || vk > 255) return false;
	return prev_[vk];
}

bool Input::ConsumePressed(int vk)
{
	if (vk < 0 || vk > 255) return false;
	if (!pressed_[vk]) return false;
	pressed_[vk] = false;
	return true;
}

} // namespace sfc
