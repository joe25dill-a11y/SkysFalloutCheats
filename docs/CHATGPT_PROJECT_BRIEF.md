# Sky's Fallout Cheats — Project Brief for AI Review

Copy everything below this line into ChatGPT (or another AI). Ask it for improvements, architecture critiques, risk fixes, and feature ideas. Then paste its reply back to your Cursor agent.

---

## What you are reviewing

**Product:** Sky's Fallout Cheats (working title)  
**Goal:** A premium in-game trainer / HUD / utility console for *Fallout: New Vegas* — not a basic MCM script mod. It should feel like a polished PC overlay toolkit (menu + live HUD + gameplay helpers).  
**Repo path (dev machine):** `E:\Projects\SkysFalloutCheats`  
**Game install:** `E:\SteamLibrary\steamapps\common\Fallout New Vegas`  
**Status:** Early but playable alpha. Core overlay works in-world. Many features are console-command wrappers with curated catalogs. ESP is incomplete/parked. Live reads (HP/AP/caps/weapon) are partially working and still fragile.

## How it runs / loads with the game

1. **Runtime dependency:** xNVSE (NVSE) must load the game. Launch via **NVSE loader** or Vortex “NVSE Play” — *not* plain `FalloutNV.exe`.
2. **Plugin type:** Native **Win32 (32-bit) C++ DLL** implementing the NVSE plugin ABI:
   - `NVSEPlugin_Query`
   - `NVSEPlugin_Load`
3. **Install location:**
   - DLL: `Data\NVSE\Plugins\SkysFalloutCheats.dll`
   - Data folder: `Data\NVSE\Plugins\SkysFalloutCheats\` (config, log, presets, locations)
4. **Deploy workflow (dev):** CMake Release Win32 build → `tools\ENABLE_SFC.bat` copies DLL + `data_package\` into the game Plugins folder (kills `FalloutNV.exe` if locking the DLL).
5. **Disable without uninstall:** `tools\DISABLE_SFC.bat` / rename DLL to `.off`.
6. **Log file:** `Data\NVSE\Plugins\SkysFalloutCheats\sfc.log`

### Boot sequence (simplified)

```
FalloutNV + xNVSE
  → loads SkysFalloutCheats.dll
  → NVSEPlugin_Load
      → ConsoleBridge (NVSEConsoleInterface::RunScriptLine2)
      → App init (config, features, search index, hotkeys)
      → MinHook Direct3DCreate9 → CreateDevice → EndScene / Reset
  → each frame EndScene:
      → if CanDrawOverlay() (player in world, not loading menu, primary backbuffer)
          → ImGui frame: HUD + MainMenu + Search + Notifications + feature HUD
```

### In-game controls

| Key | Action |
|-----|--------|
| INSERT | Toggle main menu |
| ESC | Close menu/search |
| F1 | Command search overlay |
| F5 / F6 / F7 | God / heal / +caps (optional convenience; not a product focus) |

Overlay is **gated** so it should not draw on title/main menu / loading in ways that break D3D state. Device state is saved/restored around ImGui.

## Tech stack

| Layer | Choice |
|-------|--------|
| Language | C++17 |
| Build | CMake, **Win32 only** (FNV is 32-bit) |
| Script extender | xNVSE 6.x lean PluginAPI (vendored header; **not** full CommonLibNVSE) |
| UI | Dear ImGui + `imgui_impl_dx9` / Win32 |
| Hooks | MinHook on D3D9 (`Direct3DCreate9` → device vtable EndScene/Reset) |
| Mutations | Mostly **Fallout console commands** via NVSE console interface |
| Live reads | Hand-rolled memory / vtable calls in `GameState.cpp` (addresses rebased from absolute `0x00400000` image base) |
| Config | JSON (`nlohmann/json`) — theme, HUD, performance, controls, presets |
| Optional future | JIP / JohnnyGuitar detected via Compat probe (not required yet) |

**Important constraint:** There is no Papyrus, no MCM XML UI, no CommonLibNVSE RTTI helpers. Deep game introspection is manual offsets + SEH (`__try/__except`) for crash isolation.

## Architecture (folders)

```
src/main.cpp              NVSE entry
src/core/                 App, Config, Log, Compat, ConsoleBridge, GameState, Gameplay gate, Features registry, Input, Hotkeys, Search
src/render/               D3D9Hook, ImGuiBackend, WorldToScreen (ESP math — incomplete)
src/ui/                   Theme, MainMenu, Hud, SearchOverlay, Notifications
src/features/             One module per menu section (Player, Weapons, Inventory, Skills, NPCs, World, Teleport, Quests, Camera, ESP, Presets, Settings, Utility, Debug)
data_package/             Shipped config/presets/locations
tools/                    Deploy / enable / disable / diagnose bats
external/                 imgui, minhook, lean xNVSE headers
deps/                     Reference NVSE source (offsets / layout hints), not linked as a lib
```

**Feature pattern:** Each feature implements `IFeature` (`Id`, `Name`, `Category`, `DrawMenu`, optional `Serialize`/`Deserialize`, search entries). Registered in `RegisterFeatures.cpp`. Main menu left rail filters by `FeatureCategory`.

## What is in the product today

### Working / usable

- ImGui menu shell (amber terminal-ish theme; CRT/scanlines/glow **off by default** after they caused visual issues)
- HUD widgets: status (LVL/HP/AP/CAPS/location), world time + XYZ, weapon/ammo line
- Live player position (used by Teleport save/copy)
- Console-driven cheats across panels
- **Inventory catalog:** aid / chems / armor / misc + packs + filter
- **Weapons catalog:** ammo types, iconic weapons, repair helpers
- **Skills panel:** all 13 skills forceav, perk add/remove list
- **Teleport:** `coc` quick places (towns), live XYZ save/go, moveto ref, locations.json
- **World:** timescale, hour, weather presets, tcl/tfc/tgm, ka/kah, unlock, etc.
- **NPCs:** kill/AI toggles, resurrect, essential FormID, scale, open teammate container
- **Player:** heal/AP/rads/level/SPECIAL/carry/god/needs
- **Presets:** JSON capture of config + feature state
- **Utility:** raw console line, config reload/save
- Deploy scripts that handle DLL file locks

### Partial / fragile

- **HP/AP current values:** max often OK; current was stuck at 0 until Fn_01+Fn_06 preference; still needs real-world validation when damaged
- **Caps count:** inventory EntryData walk improved (FormID + ExtraCount stacks); still may miscount edge cases
- **Weapon/ammo HUD:** uses BaseProcess vtable GetWeaponInfo/GetAmmoInfo + offsets; fists OK; equipped guns may still be wrong
- **ESP:** parked. Wanted world AABB boxes / PD3-style mesh wrap. Camera/W2S never reliably found NiCamera (`eng`/`cam` null; D3D matrix fallback glued boxes to view). Do not treat ESP as shippable
- Many FormIDs / `coc` cell names are vanilla guesses — some may fail (DLC, typos, renamed cells)
- God mode / tcl checkboxes can **desync** from real toggle state (console toggles are fire-and-forget)
- No true “selected actor” API yet — NPC actions that need a console-picked ref are awkward

### Explicitly not done / future candidates

- Live inventory browser (read player container and list items with names/counts)
- Companion management beyond crude console
- Quest list with live stages (currently FormID/setstage helpers)
- Armor-specific panel (armor is under Inventory catalog)
- Mesh/bone ESP or true world-space overlays
- Vortex FOMOD / Nexus packaging
- Hardening against TTW / heavily modded load orders
- Safer address resolution (signature scan vs fixed FNV.exe offsets)
- Separating “read path” vs “write path” so HUD doesn’t depend on fragile AV calling conventions

## Known technical landmines (tell reviewers)

1. **Fixed absolute addresses** for GOG/Steam FNV 1.4.0.525-style EXE assumed; rebasing = `moduleBase + (abs - 0x400000)`. Different EXE builds break GameState.
2. **ActorValueOwner float returns** — some vfuncs return float in ST0, some bit-pattern in EAX; wrong convention → `0` HP.
3. **ExtraContainerChanges::EntryData** layout: `extendData@0`, `countDelta@4`, `type@8` (NVSE). Wrong offsets = bad caps/ammo counts.
4. **D3D9 EndScene** can run for multiple swapchains; overlay must only draw on primary backbuffer + restore state or the game UI stretches/breaks.
5. **Drawing before player/world exists** historically crashed or wrecked menus — hence `CanDrawOverlay()`.
6. Deploy while game is running fails silently if DLL locked — users often tested **old builds**.

## Product quality bar (author intent)

- Premium feel: one coherent utility console, not a spammy cheat dump
- Safe-by-default visuals (no heavy CRT)
- Prefer curated named actions over raw FormID typing (but keep custom FormID escape hatches)
- Live HUD should be trustworthy (HP/AP/caps/weapon) before more ESP toys
- Extensible feature modules for a large toolkit over time

## Questions to ask the reviewing AI

Please answer specifically:

1. What are the **highest-risk bugs / architectural mistakes** in this design?
2. What should be the **next 5 improvements** ranked by user value vs engineering risk?
3. How would you redesign **live game-state reading** (HP, inventory, weapon) more safely without CommonLib?
4. Should mutations stay on **console commands**, or move to direct native calls — tradeoffs for FNV?
5. What would make this feel more “premium” in UI/UX without rewriting the engine?
6. How should **ESP** be approached later (or abandoned) given NiCamera / W2S pain?
7. Packaging / distribution advice for Vortex + NVSE users.
8. Any **security / stability** practices for an injected overlay DLL in a 32-bit 2010 engine.

Be concrete. Prefer actionable recommendations over generic “add tests” advice. Call out anything that sounds unsafe to ship.

## Optional prompt the user can append

> I am not an expert. Critique this project as a senior FNV / reverse-engineering / ImGui overlay engineer. Give me a prioritized backlog I can paste back into Cursor for implementation. Mark items as Fix / Improve / Add / Defer.
