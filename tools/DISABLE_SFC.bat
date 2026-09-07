@echo off
set "G=E:\SteamLibrary\steamapps\common\Fallout New Vegas\Data\NVSE\Plugins"
echo Disabling Sky's Fallout Cheats...
if exist "%G%\SkysFalloutCheats.dll" (
  move /Y "%G%\SkysFalloutCheats.dll" "%G%\SkysFalloutCheats.dll.off"
  echo DISABLED.
) else (
  echo DLL already gone or already .off
)
dir /b "%G%\SkysFalloutCheats*"
