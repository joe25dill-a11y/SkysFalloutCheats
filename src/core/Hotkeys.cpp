#include "core/Hotkeys.hpp"
#include "core/Input.hpp"
#include "core/Log.hpp"
#include <windows.h>
#include <iterator>

namespace sfc {

Hotkeys& Hotkeys::Get()
{
	static Hotkeys instance;
	return instance;
}

void Hotkeys::Bind(const HotkeyBinding& b)
{
	bindings_[b.actionId] = b;
}

void Hotkeys::Clear(const std::string& actionId)
{
	bindings_.erase(actionId);
}

const HotkeyBinding* Hotkeys::Find(const std::string& actionId) const
{
	auto it = bindings_.find(actionId);
	return it == bindings_.end() ? nullptr : &it->second;
}

std::vector<std::string> Hotkeys::Conflicts() const
{
	std::vector<std::string> out;
	for (auto a = bindings_.begin(); a != bindings_.end(); ++a) {
		for (auto b = std::next(a); b != bindings_.end(); ++b) {
			if (a->second.vk == b->second.vk &&
				a->second.ctrl == b->second.ctrl &&
				a->second.shift == b->second.shift &&
				a->second.alt == b->second.alt &&
				a->second.vk != 0) {
				out.push_back(a->first + " vs " + b->first);
			}
		}
	}
	return out;
}

void Hotkeys::SetCallback(const std::string& actionId, Callback cb)
{
	callbacks_[actionId] = cb;
}

void Hotkeys::Tick()
{
	auto& input = Input::Get();
	if (input.MenuOpen() || input.SearchOpen()) return;

	for (auto& [id, b] : bindings_) {
		if (b.vk == 0) continue;
		if (b.ctrl && !(GetAsyncKeyState(VK_CONTROL) & 0x8000)) continue;
		if (b.shift && !(GetAsyncKeyState(VK_SHIFT) & 0x8000)) continue;
		if (b.alt && !(GetAsyncKeyState(VK_MENU) & 0x8000)) continue;
		if (!input.KeyPressed(b.vk)) continue;
		auto it = callbacks_.find(id);
		if (it != callbacks_.end() && it->second) it->second();
	}
}

} // namespace sfc
