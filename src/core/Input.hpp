#pragma once
#include <windows.h>

namespace sfc {

class Input {
public:
	static Input& Get();
	void Tick();
	bool KeyPressed(int vk) const;
	bool KeyDown(int vk) const;
	bool ConsumePressed(int vk);

	bool MenuOpen() const { return menuOpen_; }
	void SetMenuOpen(bool v) { menuOpen_ = v; }
	void ToggleMenu() { menuOpen_ = !menuOpen_; }

	bool SearchOpen() const { return searchOpen_; }
	void SetSearchOpen(bool v) { searchOpen_ = v; }
	void ToggleSearch() { searchOpen_ = !searchOpen_; }

	bool WantCapture() const { return menuOpen_ || searchOpen_; }

private:
	bool menuOpen_ = false;
	bool searchOpen_ = false;
	bool prev_[256]{};
	bool pressed_[256]{};
};

} // namespace sfc
