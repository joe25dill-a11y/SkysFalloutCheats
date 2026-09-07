#include "core/NvseMessages.hpp"
#include "core/App.hpp"
#include "core/Boot.hpp"
#include "core/Diag.hpp"
#include "core/GameState.hpp"
#include "core/GameWorkQueue.hpp"
#include "core/Log.hpp"

namespace sfc {
namespace {

void OnNvseMessage(NVSEMessagingInterface::Message* msg)
{
	if (!msg) return;

	switch (msg->type) {
	case NVSEMessagingInterface::kMessage_MainGameLoop:
		if (App::Get().Ready())
			App::Get().TickGameWorld();
		break;

	case NVSEMessagingInterface::kMessage_PreLoadGame:
	case NVSEMessagingInterface::kMessage_ExitToMainMenu:
	case NVSEMessagingInterface::kMessage_ExitGame:
	case NVSEMessagingInterface::kMessage_ExitGame_Console:
		DiagEvent("nvse.load_or_exit", "invalidate");
		ResetWorldSettle("nvse_exit_or_preload");
		GameState::Get().Invalidate("nvse_exit_or_preload");
		break;

	case NVSEMessagingInterface::kMessage_PostLoadGame:
		DiagEvent("nvse.post_load_game", nullptr);
		ResetWorldSettle("nvse_post_load");
		GameState::Get().Invalidate("nvse_post_load");
		break;

	// NOTE: Do NOT Invalidate on OnCellStateChange — it fires dozens of times
	// during load/stream and flooded the log (and destabilized the last crash session).
	// Cell gating stays in CanDrawOverlay / exterior_stream soft invalidate.

	default:
		break;
	}
}

} // namespace

bool InstallNvseMessages(PluginHandle handle, NVSEInterface* nvse)
{
	if (!nvse || !nvse->QueryInterface) {
		GameWorkQueue::Get().SetGameLoopAvailable(false);
		return false;
	}

	auto* msg = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));
	if (!msg || !msg->RegisterListener) {
		SFC_WARN("[NVSE] Messaging interface unavailable");
		GameWorkQueue::Get().SetGameLoopAvailable(false);
		return false;
	}

	if (!msg->RegisterListener(handle, "NVSE", &OnNvseMessage)) {
		SFC_ERR("[NVSE] RegisterListener(NVSE) failed");
		GameWorkQueue::Get().SetGameLoopAvailable(false);
		return false;
	}

	GameWorkQueue::Get().SetGameLoopAvailable(true);
	SFC_LOG("[NVSE] Messaging registered — MainGameLoop = mutations only; HUD reads on EndScene");
	return true;
}

} // namespace sfc
