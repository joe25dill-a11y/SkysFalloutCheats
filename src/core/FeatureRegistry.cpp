#include "core/FeatureRegistry.hpp"
#include "core/Log.hpp"

namespace sfc {

FeatureRegistry& FeatureRegistry::Get()
{
	static FeatureRegistry instance;
	return instance;
}

void FeatureRegistry::Register(std::unique_ptr<IFeature> feature)
{
	if (!feature) return;
	byId_[feature->Id()] = feature.get();
	features_.push_back(std::move(feature));
}

bool FeatureRegistry::InitAll()
{
	bool ok = true;
	for (auto& f : features_) {
		try {
			if (!f->Init()) {
				SFC_WARN("Feature init failed: %s", f->Id());
				ok = false;
			} else {
				SFC_LOG("Feature ready: %s", f->Id());
			}
		} catch (...) {
			SFC_ERR("Feature init exception: %s", f->Id());
			ok = false;
		}
	}
	return ok;
}

void FeatureRegistry::ShutdownAll()
{
	for (auto it = features_.rbegin(); it != features_.rend(); ++it) {
		(*it)->Shutdown();
	}
}

void FeatureRegistry::TickAll(float dt)
{
	for (auto& f : features_) {
		if (f->IsAvailable()) f->Tick(dt);
	}
}

void FeatureRegistry::DrawHudAll()
{
	for (auto& f : features_) {
		if (f->IsAvailable()) f->DrawHud();
	}
}

IFeature* FeatureRegistry::Find(const char* id)
{
	auto it = byId_.find(id);
	return it == byId_.end() ? nullptr : it->second;
}

std::vector<IFeature*> FeatureRegistry::ByCategory(FeatureCategory cat)
{
	std::vector<IFeature*> out;
	for (auto& f : features_) {
		if (f->Category() == cat) out.push_back(f.get());
	}
	return out;
}

} // namespace sfc
