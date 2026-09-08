#include "core/FeatureRegistry.hpp"
#include "core/Log.hpp"
#include <memory>

namespace sfc {

std::unique_ptr<IFeature> CreatePlayerFeature();
std::unique_ptr<IFeature> CreateWeaponsFeature();
std::unique_ptr<IFeature> CreateInventoryFeature();
std::unique_ptr<IFeature> CreateSkillsFeature();
std::unique_ptr<IFeature> CreateNpcFeature();
std::unique_ptr<IFeature> CreateWorldFeature();
std::unique_ptr<IFeature> CreateTeleportFeature();
std::unique_ptr<IFeature> CreateQuestFeature();
std::unique_ptr<IFeature> CreateCameraFeature();
std::unique_ptr<IFeature> CreateEspFeature();
std::unique_ptr<IFeature> CreateGrabFeature();
std::unique_ptr<IFeature> CreateAimbotFeature();
std::unique_ptr<IFeature> CreatePresetsFeature();
std::unique_ptr<IFeature> CreateSettingsFeature();
std::unique_ptr<IFeature> CreateUtilityFeature();
std::unique_ptr<IFeature> CreateDebugFeature();

void RegisterBuiltinFeatures()
{
	auto& reg = FeatureRegistry::Get();
	reg.Register(CreatePlayerFeature());
	reg.Register(CreateWeaponsFeature());
	reg.Register(CreateInventoryFeature());
	reg.Register(CreateSkillsFeature());
	reg.Register(CreateNpcFeature());
	reg.Register(CreateWorldFeature());
	reg.Register(CreateTeleportFeature());
	reg.Register(CreateQuestFeature());
	reg.Register(CreateCameraFeature());
	reg.Register(CreateEspFeature());
	reg.Register(CreateGrabFeature());
	reg.Register(CreateAimbotFeature());
	reg.Register(CreatePresetsFeature());
	reg.Register(CreateSettingsFeature());
	reg.Register(CreateUtilityFeature());
	reg.Register(CreateDebugFeature());
	SFC_LOG("Registered %zu builtin features", reg.All().size());
}

} // namespace sfc
