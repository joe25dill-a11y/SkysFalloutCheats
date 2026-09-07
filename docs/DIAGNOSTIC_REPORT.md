# DIAGNOSTIC REPORT — Sky's Fallout Cheats

Date: 2026-09-07
Mode: Isolation Test A forced (diagnostics.isolationMode = static_imgui)
Build: diagnostic-isolation (deployed ~5:21 PM)

## ROOT CAUSE CANDIDATES
1. D3D9/ImGui EndScene state fight (most likely for vanilla HUD flicker) — user: Fallout HUD glitches when SFC overlay comes on.
2. Double StateBlock (ours + imgui_impl_dx9 every RenderDrawData).
3. Unsafe GameState/console near render (more crash/freeze than flicker).

## WHAT YOU TESTED
Test A ACTIVE NOW: static ImGui only. Off: GameState, console, companions, teleports, INSERT, hotkeys, CanDrawOverlay player/cell walks.

Other modes (config isolationMode): no_gamestate | no_console | no_companions | off

## WHAT FAILED (history)
Random patches; Present hook crashed NVSE; HUD strip/every-other-frame; flicker persisted with overlay on.

## WHAT WORKED (partial)
Deferred console to MainGameLoop; settle timer; heavy-cmd rate limit — not flicker.

## EXACT FILES FOR NEXT D3D FIX (if Test A still flickers)
- src/render/D3D9Hook.cpp
- src/render/ImGuiBackend.cpp
- external/imgui/backends/imgui_impl_dx9.cpp

## NEXT PATCH
1. Launch NVSE, confirm log [ISOLATION] mode=static_imgui and on-screen SFC DIAG.
2. Walk / Pip-Boy / combat.
3. Reply: Fallout vanilla HUD still glitch YES or NO.
4. Do not unlock features until that answer.

Set isolationMode to off later to restore full product (features gated, not deleted).
