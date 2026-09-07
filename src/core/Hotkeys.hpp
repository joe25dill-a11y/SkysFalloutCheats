#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace sfc {

struct HotkeyBinding {
	std::string actionId;
	std::string label;
	int vk = 0;
	bool ctrl = false;
	bool shift = false;
	bool alt = false;
	bool toggle = true;
};

class Hotkeys {
public:
	static Hotkeys& Get();
	void Bind(const HotkeyBinding& b);
	void Clear(const std::string& actionId);
	const HotkeyBinding* Find(const std::string& actionId) const;
	std::vector<std::string> Conflicts() const;
	void Tick(); // fires actions via registered callbacks
	using Callback = void(*)();
	void SetCallback(const std::string& actionId, Callback cb);

private:
	std::unordered_map<std::string, HotkeyBinding> bindings_;
	std::unordered_map<std::string, Callback> callbacks_;
};

} // namespace sfc
