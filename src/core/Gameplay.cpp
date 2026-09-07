#include "core/Gameplay.hpp"
#include "core/Boot.hpp"
#include "core/GameState.hpp"
#include "core/Log.hpp"
#include <windows.h>
#include <cstdint>
#include <cstddef>

namespace sfc {
namespace {

constexpr std::uintptr_t kPreferredBase = 0x00400000;
constexpr std::uintptr_t kThePlayerAbs = 0x011DEA3C;
constexpr std::uintptr_t kMenuVisibilityAbs = 0x011F308F;

// From xNVSE GameUI.h
constexpr std::uint32_t kMenuType_Message = 0x3E9;
constexpr std::uint32_t kMenuType_Inventory = 0x3EA;
constexpr std::uint32_t kMenuType_Stats = 0x3EB;
constexpr std::uint32_t kMenuType_Loading = 0x3EF;
constexpr std::uint32_t kMenuType_Container = 0x3F0;
constexpr std::uint32_t kMenuType_Dialog = 0x3F1;
constexpr std::uint32_t kMenuType_SleepWait = 0x3F4;
constexpr std::uint32_t kMenuType_Start = 0x3F5;
constexpr std::uint32_t kMenuType_LockPick = 0x3F6;
constexpr std::uint32_t kMenuType_Map = 0x3FF;
constexpr std::uint32_t kMenuType_LevelUp = 0x403;
constexpr std::uint32_t kMenuType_Repair = 0x40B;
constexpr std::uint32_t kMenuType_Barter = 0x41D;
constexpr std::uint32_t kMenuType_Hacking = 0x41F;
constexpr std::uint32_t kMenuType_VATS = 0x420;
constexpr std::uint32_t kMenuType_Computers = 0x421;

constexpr std::ptrdiff_t kOff_ParentCell = 0x040;
constexpr std::ptrdiff_t kOff_CellFlags = 0x024; // TESObjectCELL::cellFlags, bit0 = interior

std::uintptr_t Rel(std::uintptr_t absAddr)
{
	const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
	return base + (absAddr - kPreferredBase);
}

bool CellIsInterior(void* cell)
{
	if (!cell) return true;
	__try {
		const auto flags = *reinterpret_cast<std::uint8_t*>(reinterpret_cast<char*>(cell) + kOff_CellFlags);
		return (flags & 0x01) != 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return true; // fail closed → treat as interior (hard settle)
	}
}

bool MenuVisible(std::uint32_t menuType)
{
	__try {
		auto* arr = reinterpret_cast<volatile std::uint8_t*>(Rel(kMenuVisibilityAbs));
		return arr[menuType] != 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void* PlayerPtr()
{
	__try {
		void** slot = reinterpret_cast<void**>(Rel(kThePlayerAbs));
		if (!slot) return nullptr;
		void* player = *slot;
		if (!player) return nullptr;
		const auto p = reinterpret_cast<std::uintptr_t>(player);
		if (p <= 0x10000 || p >= 0xFFF00000) return nullptr;
		return player;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return nullptr;
	}
}

bool PlayerCell(void* player, void** outCell)
{
	if (outCell) *outCell = nullptr;
	if (!player) return false;
	__try {
		void* cell = *reinterpret_cast<void**>(reinterpret_cast<char*>(player) + kOff_ParentCell);
		if (!cell) return false;
		const auto c = reinterpret_cast<std::uintptr_t>(cell);
		if (c <= 0x10000 || c >= 0xFFF00000) return false;
		if (outCell) *outCell = cell;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

} // namespace

bool HasPlayer()
{
	return PlayerPtr() != nullptr;
}

bool IsLoadingScreen()
{
	return MenuVisible(kMenuType_Loading);
}

bool IsGameMenuBlocking()
{
	// Drawing ImGui over Pip-Boy / containers is a common crash path.
	static const std::uint32_t kBlock[] = {
		kMenuType_Inventory,
		kMenuType_Stats,
		kMenuType_Map,
		kMenuType_Container,
		kMenuType_Dialog,
		kMenuType_SleepWait,
		kMenuType_Start,
		kMenuType_LockPick,
		kMenuType_LevelUp,
		kMenuType_Repair,
		kMenuType_Barter,
		kMenuType_Hacking,
		kMenuType_VATS,
		kMenuType_Computers,
		kMenuType_Message,
	};
	for (auto t : kBlock) {
		if (MenuVisible(t)) return true;
	}
	return false;
}

bool CanDrawOverlay()
{
	// Only edge-trigger settle resets — logging/resetting every EndScene during
	// loads flooded the log and hammered disk I/O (freeze/crash risk).
	static bool s_wasLoading = false;
	static void* s_lastCell = nullptr;

	const bool loading = IsLoadingScreen();
	if (loading) {
		if (!s_wasLoading)
			ResetWorldSettle("loading_menu");
		s_wasLoading = true;
		return false;
	}
	s_wasLoading = false;

	if (IsGameMenuBlocking())
		return false;

	void* player = PlayerPtr();
	if (!player) {
		static bool logged = false;
		if (!logged) { ResetWorldSettle("no_player"); logged = true; }
		return false;
	}

	void* cell = nullptr;
	if (!PlayerCell(player, &cell)) {
		ResetWorldSettle("no_cell");
		s_lastCell = nullptr;
		return false;
	}

	if (cell != s_lastCell) {
		const void* prev = s_lastCell;
		s_lastCell = cell;

		const bool prevInterior = CellIsInterior(const_cast<void*>(prev));
		const bool nowInterior = CellIsInterior(cell);
		// Exterior grid streaming while running is normal — do NOT 4s-kill the HUD.
		// Hard settle only for doors / interior transitions / first cell.
		const bool hardTransition = (prev == nullptr) || prevInterior || nowInterior;
		if (hardTransition) {
			ResetWorldSettle(prevInterior || nowInterior ? "interior_transition" : "cell_changed");
			GameState::Get().Invalidate("cell_changed");
			return false;
		}

		// Same world exterior → exterior: keep overlay armed, soft-refresh reads.
		GameState::Get().Invalidate("exterior_stream");
	}

	return ObserveWorldReady();
}

namespace {
bool g_overlayGateCached = false;
}

void RefreshOverlayGateCache()
{
	g_overlayGateCached = CanDrawOverlay();
}

bool OverlayGateCached()
{
	return g_overlayGateCached;
}

} // namespace sfc
