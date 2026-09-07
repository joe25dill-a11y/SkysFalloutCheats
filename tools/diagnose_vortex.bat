@echo off
set "G=E:\SteamLibrary\steamapps\common\Fallout New Vegas"
echo === sfc.log ===
if exist "%G%\Data\NVSE\Plugins\SkysFalloutCheats\sfc.log" (
  type "%G%\Data\NVSE\Plugins\SkysFalloutCheats\sfc.log"
) else echo MISSING sfc.log
echo.
echo === nvse.log plugin lines ===
powershell -NoProfile -Command "if (Test-Path '%G%\nvse.log') { Select-String -Path '%G%\nvse.log' -Pattern 'SkysFallout|plugin|fatal|disabled|ImGui|D3D' | Select-Object -Last 40 }"
echo.
echo === Vortex falloutnv ===
dir /b "%USERPROFILE%\AppData\Roaming\Vortex\falloutnv" 2>nul
powershell -NoProfile -Command "$p='$env:APPDATA\Vortex\falloutnv'; if (Test-Path $p) { Get-ChildItem $p -Recurse -Filter '*Skys*' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName; Get-Content (Join-Path $p 'openvortex') -ErrorAction SilentlyContinue }"
echo.
echo === DLL still in game Plugins? ===
dir "%G%\Data\NVSE\Plugins\SkysFalloutCheats.dll"
echo.
echo === steam_loader log tail ===
powershell -NoProfile -Command "if (Test-Path '%G%\nvse_steam_loader.log') { Get-Content '%G%\nvse_steam_loader.log' -Tail 30 }"
