#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <functional>

namespace sfc {

enum class FeatureCategory {
	Player, Weapons, Armor, Inventory, Npcs, World, Map, Teleport,
	Quests, Perks, Skills, Gameplay, Visuals, Utility, Presets,
	Settings, Debug, Camera, Esp, Search, Favorites
};

inline const char* CategoryName(FeatureCategory c)
{
	switch (c) {
	case FeatureCategory::Player: return "PLAYER";
	case FeatureCategory::Weapons: return "WEAPONS";
	case FeatureCategory::Armor: return "ARMOR";
	case FeatureCategory::Inventory: return "INVENTORY";
	case FeatureCategory::Npcs: return "NPCs";
	case FeatureCategory::World: return "WORLD";
	case FeatureCategory::Map: return "MAP";
	case FeatureCategory::Teleport: return "TELEPORT";
	case FeatureCategory::Quests: return "QUESTS";
	case FeatureCategory::Perks: return "PERKS";
	case FeatureCategory::Skills: return "SKILLS";
	case FeatureCategory::Gameplay: return "GAMEPLAY";
	case FeatureCategory::Visuals: return "VISUALS";
	case FeatureCategory::Utility: return "UTILITY";
	case FeatureCategory::Presets: return "PRESETS";
	case FeatureCategory::Settings: return "SETTINGS";
	case FeatureCategory::Debug: return "DEBUG";
	case FeatureCategory::Camera: return "CAMERA";
	case FeatureCategory::Esp: return "ESP";
	case FeatureCategory::Search: return "SEARCH";
	case FeatureCategory::Favorites: return "FAVORITES";
	}
	return "UNKNOWN";
}

struct SearchEntry {
	std::string id;
	std::string title;
	std::string keywords;
	FeatureCategory category = FeatureCategory::Utility;
	std::function<void()> activate;
};

class IFeature {
public:
	virtual ~IFeature() = default;
	virtual const char* Id() const = 0;
	virtual const char* Name() const = 0;
	virtual FeatureCategory Category() const = 0;
	virtual bool Init() { return true; }
	virtual void Shutdown() {}
	virtual void Tick(float /*dt*/) {}
	virtual void DrawMenu() {}
	virtual void DrawHud() {}
	virtual bool IsAvailable() const { return available_; }
	virtual const char* UnavailableReason() const { return unavailableReason_.c_str(); }
	virtual void CollectSearch(std::vector<SearchEntry>& /*out*/) {}
	virtual nlohmann::json Serialize() const { return {}; }
	virtual void Deserialize(const nlohmann::json& /*j*/) {}

protected:
	bool available_ = true;
	std::string unavailableReason_;
};

} // namespace sfc
