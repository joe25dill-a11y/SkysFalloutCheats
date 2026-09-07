# Troubleshooting

## Menu does not open

- Confirm `nvse_loader.exe` / steam loader is used
- Check `Data\NVSE\Plugins\SkysFalloutCheats.dll` exists
- Read `SkysFalloutCheats\sfc.log`
- Ensure DLL is 32-bit

## Features say unavailable

Console interface missing or plugin loaded in editor context. Update xNVSE.

## Crash on alt-tab / resolution change

Reset hook should recreate ImGui device objects. If not, report with log + GPU/overlay list.

## Overlay conflict

Disable conflicting overlays temporarily (Discord/Steam/ENB) to test.
