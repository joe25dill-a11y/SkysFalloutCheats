# Architecture

## Stack

- xNVSE plugin DLL (`SkysFalloutCheats.dll`)
- Dear ImGui + Win32/DX9 backends
- MinHook on `IDirect3DDevice9::EndScene` / `Reset`
- nlohmann/json for config, presets, locations
- Console bridge (`NVSEConsoleInterface::RunScriptLine2`) for safe game mutations

## Modules

```
Core     init, config, log, input, hotkeys, search index, compat, feature registry
Render   D3D9 hooks, ImGui bootstrap
UI       theme, main menu, HUD widgets, notifications, search overlay
Features player, weapons, inventory, npc, world, teleport, quest, camera, esp, presets, settings, utility, debug
Data     JSON under Data/NVSE/Plugins/SkysFalloutCheats/
```

## Feature contract

Implement `sfc::IFeature`:

- `Id`, `Name`, `Category`
- `Init` / `Shutdown` / `Tick`
- `DrawMenu` / `DrawHud`
- `CollectSearch`
- Soft-fail via `available_` + `unavailableReason_`

Register in `RegisterBuiltinFeatures()`.

## Frame flow

1. EndScene hook
2. ImGui NewFrame
3. Input + hotkeys
4. Feature ticks
5. HUD → Main menu → Search → Notifications
6. ImGui Render

## Compatibility

- Hard dependency: xNVSE
- Soft: JIP / JohnnyGuitar (detected, feature-gated later)
- Overlay conflicts (ENB/ReShade/Steam): hooks may need rechain — monitor if EndScene stops firing
