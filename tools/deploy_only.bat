@echo off
set "GAME=E:\SteamLibrary\steamapps\common\Fallout New Vegas"
set "PLUG=%GAME%\Data\NVSE\Plugins"
mkdir "%PLUG%\SkysFalloutCheats" 2>nul
copy /Y "E:\Projects\SkysFalloutCheats\build\bin\Release\SkysFalloutCheats.dll" "%PLUG%\SkysFalloutCheats.dll"
xcopy /E /I /Y "E:\Projects\SkysFalloutCheats\data_package\*" "%PLUG%\SkysFalloutCheats\"
dir "%GAME%\nvse_loader.exe"
dir "%PLUG%\SkysFalloutCheats.dll"
dir "%PLUG%\SkysFalloutCheats"
