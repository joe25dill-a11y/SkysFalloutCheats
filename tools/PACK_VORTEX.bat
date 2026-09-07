@echo off
setlocal
set "ROOT=E:\Projects\SkysFalloutCheats"
set "DLL=%ROOT%\build\bin\Release\SkysFalloutCheats.dll"
set "OUT=%ROOT%\dist\SkysFalloutCheats_Vortex"
set "ZIP=%ROOT%\dist\SkysFalloutCheats_Vortex.zip"

if not exist "%DLL%" (
  echo ERROR: Build the Release DLL first.
  echo Missing: %DLL%
  exit /b 1
)

echo Building Vortex/manual install folder...
if exist "%OUT%" rmdir /S /Q "%OUT%"
mkdir "%OUT%\Data\NVSE\Plugins\SkysFalloutCheats" 2>nul

copy /Y "%DLL%" "%OUT%\Data\NVSE\Plugins\SkysFalloutCheats.dll" >nul
xcopy /E /I /Y "%ROOT%\data_package\*" "%OUT%\Data\NVSE\Plugins\SkysFalloutCheats\" >nul

(
  echo Sky's Fallout Cheats — Vortex / Manual Install
  echo.
  echo Requirements: xNVSE 6.4+ installed in Fallout New Vegas.
  echo.
  echo Vortex: drop this archive as a mod ^(staging path Data\...^).
  echo Manual: copy the Data folder into your Fallout New Vegas directory.
  echo.
  echo Launch with nvse_loader / Vortex NVSE Play — not plain FalloutNV.exe.
  echo.
  echo Controls: INSERT menu, F1 search, ESC close.
  echo HUD pauses during Pip-Boy and for ~4s after doors/loads.
) > "%OUT%\README_INSTALL.txt"

if exist "%ZIP%" del /F /Q "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%OUT%\*' -DestinationPath '%ZIP%' -Force"
echo.
echo Packed:
dir "%ZIP%"
echo Folder: %OUT%
echo Done.
endlocal
