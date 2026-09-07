# Sky's Fallout Cheats

Premium Fallout: New Vegas utility console / trainer / HUD toolkit.

Built as an **xNVSE C++ plugin** with **Dear ImGui** (Direct3D 9).

## Requirements

- Fallout: New Vegas (Steam/GOG), patched for 4GB recommended
- [xNVSE 6.4+](https://github.com/xNVSE/NVSE/releases) (installed into the game folder)
- Visual Studio 2022 (Desktop C++) to build from source
- CMake 3.21+

Optional (soft-detected, not required):
- JIP LN NVSE
- JohnnyGuitar NVSE

## Controls (defaults)

| Key | Action |
|-----|--------|
| **INSERT** | Toggle main utility menu |
| **F1** | Command search |
| **ESC** | Close menu / search |

HUD corner widgets pause while Pip-Boy / pause menus are open, and for ~4 seconds after door/cell loads (stability).

Change bindings in **SETTINGS** or `config.json`.

## Install (built DLL)

1. Install xNVSE into the Fallout New Vegas folder.
2. Copy `SkysFalloutCheats.dll` to `Data\NVSE\Plugins\`.
3. Copy the `data_package` contents to `Data\NVSE\Plugins\SkysFalloutCheats\`.
4. Launch with `nvse_loader.exe` (or Vortex **NVSE Play**).

Or run `tools\ENABLE_SFC.bat` after a Release build (dev machine paths), or `tools\PACK_VORTEX.bat` to make a zip under `dist\`.

## Build

```bat
set FalloutNVPath=E:\SteamLibrary\steamapps\common\Fallout New Vegas
cmake -S . -B build -A Win32
cmake --build build --config Release
```

**Important:** the plugin must be **Win32 (x86)**. x64 builds will not load.

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/ADDING_FEATURES.md](docs/ADDING_FEATURES.md).

## Safety

- Game mutations go through the NVSE console bridge when available.
- Missing dependencies disable features with a clear UI message instead of crashing.
- Config/presets/locations live on disk as JSON (not embedded in saves).

## Logs

`Data\NVSE\Plugins\SkysFalloutCheats\sfc.log`
