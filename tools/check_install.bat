@echo off
set "G=E:\SteamLibrary\steamapps\common\Fallout New Vegas"
echo === NVSE files ===
dir /b "%G%\nvse_*" 2>nul
echo.
echo === Plugins folder ===
dir /b "%G%\Data\NVSE\Plugins" 2>nul
echo.
echo === SFC data ===
dir /b "%G%\Data\NVSE\Plugins\SkysFalloutCheats" 2>nul
echo.
if exist "%G%\Data\NVSE\Plugins\SkysFalloutCheats\sfc.log" (
  echo === LOG ===
  type "%G%\Data\NVSE\Plugins\SkysFalloutCheats\sfc.log"
) else (
  echo NO_LOG_YET — plugin never started
)
if exist "%G%\Data\NVSE\nvse.log" (
  echo.
  echo === NVSE LOG tail ===
  powershell -NoProfile -Command "Get-Content -Path '%G%\Data\NVSE\nvse.log' -Tail 40"
)
if exist "%G%\nvse.log" (
  echo.
  echo === root nvse.log tail ===
  powershell -NoProfile -Command "Get-Content -Path '%G%\nvse.log' -Tail 40"
)
