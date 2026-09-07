# Sky's Fallout Cheats — Full Brief for ChatGPT (Crash / Freeze Fix)

**Copy this entire file into ChatGPT.** Ask: *Given this FNV NVSE ImGui trainer, why does the game freeze/crash while running or after play, and what concrete code/architecture changes should we make so live HUD + menu + teleports stay stable?*

Then paste ChatGPT’s reply back into Cursor.

---

## 1. Product

| Item | Value |
|------|--------|
| Name | Sky's Fallout Cheats (SFC) |
| Game | Fallout: New Vegas (Steam), 32-bit |
| Game path | `E:\SteamLibrary\steamapps\common\Fallout New Vegas` |
| Repo | `E:\Projects\SkysFalloutCheats` |
| Type | **xNVSE Win32 C++ DLL plugin** + Dear ImGui (D3D9) overlay |
| Not | MCM / Papyrus / CommonLibNVSE |

**Goal:** Premium in-game trainer: always-on corner HUD + INSERT options menu + teleports, companions, inventories, etc.

---

## 2. How it loads / runs

1. User launches via **NVSE** (not raw `FalloutNV.exe`).
2. xNVSE loads `Data\NVSE\Plugins\SkysFalloutCheats.dll`.
3. `NVSEPlugin_Query` / `NVSEPlugin_Load`:
   - Grab `NVSEConsoleInterface` → `RunScriptLine2` (all cheats are mostly console lines).
   - `App::Init` → config JSON, feature registry, hotkeys.
   - MinHook: `Direct3DCreate9` → `CreateDevice` → hook device **EndScene** + **Reset**.
4. Deploy: CMake **Win32 Release** → `tools\ENABLE_SFC.bat` copies DLL + `data_package\` into Plugins.
5. Log: `Data\NVSE\Plugins\SkysFalloutCheats\sfc.log`
6. Disable: rename DLL / `DISABLE_SFC.bat`.

### Controls

| Key | Action |
|-----|--------|
| INSERT | Full options menu |
| ESC | Close menu |
| F1 | Search |
| (optional) F5/F6/F7 | God / heal / caps |

**Two UI layers (must not confuse):**
- **Live HUD** — always-on corner stats (HP/AP/caps/weapon). Should stay visible while playing.
- **INSERT menu** — full cheat panels. Only when toggled.

---

## 3. Per-frame pipeline (critical)

Every `EndScene` (`src/render/D3D9Hook.cpp`):

```
hkEndScene:
  if LoadingMenu or Pip-Boy/etc → CloseAllUi()
  Input::Tick()                    // INSERT / ESC
  App::TickBackground()            // companion follow + deferred teleport console cmds
                                   // SKIPPED while LoadingMenu
  canDraw = CanDrawOverlay()       // see gate below
  if canDraw → Hotkeys::Tick()
  wantUi = menuOpen OR liveHud
  if canDraw && wantUi && primary backbuffer:
      StateBlock capture
      ImGui NewFrame
      App::OnFrame()               // GameState::Tick + Hud + MainMenu + ...
      ImGui Render
      StateBlock restore + transform restore
```

### `CanDrawOverlay()` (`src/core/Gameplay.cpp`)

Returns false when:
- Loading menu up
- Blocking game UI (Inventory/Stats/Map/Container/Dialog/VATS/etc.)
- No player / no cell
- **Hard cell transition** → starts ~**4s wall-clock settle** (`Boot.cpp` `kSettleMs = 4000`)

**Recent fix (still crashing afterward):**
- **Exterior → exterior** grid cells while running: do **NOT** full 4s settle (was hammering HUD every Mojave cell).
- **Interior / door / load / first cell:** still hard settle + `GameState::Invalidate`.

Interior test: `TESObjectCELL+0x24` bit0 = interior (NVSE-style).

---

## 4. Major systems

### Live reads — `GameState.cpp`
- Rebased addresses: `moduleBase + (abs - 0x400000)`.
- Reads player HP/AP/level, caps (container extras), GameHour, cell name, weapon/ammo.
- Wrapped in `__try/__except`.
- Refresh ~0.5s while overlay draws.
- NaN XYZ → hard-disable overlay draws.
- Keep last good HUD values on brief fail.

### Mutations — `ConsoleBridge`
- `NVSEConsoleInterface::RunScriptLine2(line, nullptr, true)`.
- Features issue `player.additem`, `coc`, `setpos`, `"ref".moveto player`, etc.

### Teleport — `TeleportFeature.cpp`
- Town buttons: **deferred `coc <EditorID>`** (~50ms after closing menu) via `TickBackground`.
- Saved locations / manual: `player.setpos x/y/z` (user trusts this more than town `coc`).
- Quest: `player.movetoqt`.
- User reports: **TP (including movetoqt ×2) did not crash**; crash/freeze happened **while running** later.

### Companions — `CompanionFollow.cpp`
- Auto after teleport (default ON): snapshot teammates from high/middle-high actor lists (teammate byte `Actor+0x18D` bit7 and/or flags), then after settle + ~1.5s stable frames: `"%08X".moveto player`.
- Manual Bring buttons on NPCs / Teleport tabs.
- **Hypothesis (debated):** auto `moveto` after `coc` correlated with later AVs — but user says TP itself didn’t crash them.

### Mouse / menu — `ImGuiBackend.cpp`
- Menu open: **one** ImGui software cursor, **ClipCursor** to game client (multi-monitor), hide OS cursor.
- Feed mouse via `GetCursorPos` + `GetAsyncKeyState` after `ImGui_ImplWin32_NewFrame` (FNV DirectInput).
- Release clip on Alt-Tab / kill focus.
- Earlier bug: INSERT opened then settle `CloseAllUi` closed it instantly — fixed (don’t close menu on settle-only).

---

## 5. Crash / freeze evidence

### Windows Event Viewer (repeated pattern)
```
Faulting application: FalloutNV.exe 1.4.0.525
Faulting module:     FalloutNV.exe   (sometimes nvse_1_4.dll)
Exception:           0xc0000005 (ACCESS_VIOLATION)
Fault offset:        0x006a55a6   (seen more than once)
```
SFC DLL is **not** named as the faulting module — engine dies under us (or after we poked it).

### `sfc.log` patterns before death
Typical session:
1. Boot → D3D hooks → overlay armed after settle.
2. Menu open/close (cursor armed/released).
3. Often: `[TELEPORT] fired` + `[COMPANIONS] post-teleport moveto issued … (stable)`.
4. Many `[GAME_READY] settle reset (cell_changed)` / later `interior_transition` / `exterior_stream`.
5. `GameState … status=VALID` heartbeats continue.
6. **No `[SHUTDOWN]`** — hard crash/freeze (no clean unload).
7. Sometimes **no new `.dmp`** → likely **freeze/hang** not always a WER AV dump.

### User reports (important)
- Crashed **while trying to kill something** / **while running** — not mid-click on TP.
- Used **moveto quest target twice**, TP worked, **then** froze/crashed later while running.
- Does **not** want features ripped out (“just turn HUD/companions off”) — wants them **working correctly**.
- Multi-monitor mouse was broken (dual cursors / free to 2nd screen) — partially fixed with clip + single ImGui cursor.
- Live HUD must stay on without needing INSERT; INSERT is options only.

---

## 6. What we already tried

| Change | Intent | Outcome |
|--------|--------|---------|
| 4s settle after doors/loads | Stop brown world / NaN XYZ | Helped door crashes; over-applied to exterior grid |
| Don’t draw over Pip-Boy/VATS/etc. | Stop menu-over-UI crashes | Good |
| StateBlock + transform restore around ImGui | D3D corruption | Helped |
| Force liveHud OFF | Stability | User hated it — HUD must stay |
| Disable auto companion moveto | Stop AV after TP | User: TP didn’t crash them; still wants companions |
| Skip weapon memory reads | Combat AV theory | Reverted — wants full HUD |
| Deferred `coc` (close menu then fire) | Stop mid-ImGui coc freeze | Mixed; early version never fired if HUD off |
| Exterior≠door settle | Stop settle spam while running | Just shipped; user still froze after |
| ClipCursor + ImGui-only cursor | Multi-monitor / dual cursor | Improved; monitor for regressions |

---

## 7. Key files

```
src/main.cpp
src/core/App.cpp / App.hpp          TickBackground + OnFrame
src/core/Boot.cpp                   4s settle timer
src/core/Gameplay.cpp               CanDrawOverlay, cell interior check
src/core/GameState.cpp              Live memory reads
src/core/CompanionFollow.cpp        Teammate scan + moveto
src/core/ConsoleBridge.cpp          RunScriptLine2
src/render/D3D9Hook.cpp             EndScene/Reset pipeline
src/render/ImGuiBackend.cpp         Cursor lock / mouse feed
src/features/TeleportFeature.cpp    coc / setpos / movetoqt
src/ui/Hud.cpp                      Corner HUD
src/ui/MainMenu.cpp                 INSERT menu
```

Build: CMake `-A Win32`, target `SkysFalloutCheats`.

Addresses (examples): `kThePlayerAbs=0x011DEA3C`, `kActorProcessMgrAbs=0x011E0E80`, menu visibility `0x011F308F`.

---

## 8. Constraints / non-negotiables for a fix

1. Keep **live corner HUD** on during normal play (OK to pause briefly on real loads/doors).
2. Keep **INSERT menu** separate and reliable (mouse clickable, locked to game window on multi-monitor).
3. Keep **teleports** (town `coc` and/or setpos + `movetoqt`).
4. Keep **companions following** SFC teleports like Pip-Boy travel (`moveto` original refs — not `placeatme` clones).
5. **Do not** “fix” by permanently disabling the product.
6. Prefer fixes that stop touching the game during unsafe frames (loading, Reset, cell attach, combat script storms).
7. Win32 / D3D9 / xNVSE only.

---

## 9. Questions for ChatGPT

1. For an FNV D3D9 EndScene ImGui overlay, what is the **safest** frame gate so HUD can stay up while **running across exterior cells**, but never draw/read during unsafe engine states?
2. Is reading actor values / weapon / process lists from EndScene inherently unsafe? Should reads move to a game main-loop hook / NVSE message / lower frequency / only when INSERT open?
3. Is `"refID".moveto player` after `coc` a known delayed crash vector? Safer alternative to bring companions (script package, `MoveTo` via engine call, delay until next save load, etc.)?
4. Fault offset `0x006a55a6` in `FalloutNV.exe` — any known function / common trainer crash cause?
5. Freeze with no dump: what typically hangs FNV with overlays (ClipCursor? console from EndScene? StateBlock every frame?)?
6. Propose a **concrete patch plan** (ordered steps + which files) that Cursor can implement — not “disable HUD.”

---

## 10. Ask ChatGPT to output

Please reply with:
1. **Most likely root cause(s)** ranked.
2. **Do / Don’t** list for EndScene overlays on Gamebryo/FNV.
3. A **step-by-step implementation plan** (file-level) to stabilize live HUD + menu + TP + companions.
4. Optional: safer companion-follow design that still feels like vanilla fast travel.
5. Any must-have NVSE hooks/messages we should use instead of EndScene-only logic.

---

*Generated for AI review — Sky's Fallout Cheats — Sep 7, 2026.*
