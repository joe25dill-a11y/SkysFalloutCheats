@echo off
set "G=E:\SteamLibrary\steamapps\common\Fallout New Vegas\Data\NVSE\Plugins"
set "SRC=E:\Projects\SkysFalloutCheats\build\bin\Release\SkysFalloutCheats.dll"
tasklist /FI "IMAGENAME eq FalloutNV.exe" 2>NUL | find /I "FalloutNV.exe" >NUL
if %ERRORLEVEL%==0 (
  echo FalloutNV.exe is running - closing it so DLL can update...
  taskkill /F /IM FalloutNV.exe >NUL 2>&1
  timeout /t 2 /nobreak >NUL
)
echo Enabling Sky's Fallout Cheats...
if exist "%G%\SkysFalloutCheats.dll.off" del /F /Q "%G%\SkysFalloutCheats.dll.off"
copy /Y "%SRC%" "%G%\SkysFalloutCheats.dll"
xcopy /E /I /Y "E:\Projects\SkysFalloutCheats\data_package\*" "%G%\SkysFalloutCheats\"
dir "%G%\SkysFalloutCheats.dll"
echo Done. Launch via NVSE.
