#pragma once
#include "core/Feature.hpp"
#include <memory>
#include <vector>
#include <unordered_map>

namespace sfc {

class FeatureRegistry {
public:
	static FeatureRegistry& Get();

	void Register(std::unique_ptr<IFeature> feature);
	bool InitAll();
	void ShutdownAll();
	void TickAll(float dt);
	void DrawHudAll();

	IFeature* Find(const char* id);
	const std::vector<std::unique_ptr<IFeature>>& All() const { return features_; }

	std::vector<IFeature*> ByCategory(FeatureCategory cat);

private:
	std::vector<std::unique_ptr<IFeature>> features_;
	std::unordered_map<std::string, IFeature*> byId_;
};

void RegisterBuiltinFeatures();

} // namespace sfc
