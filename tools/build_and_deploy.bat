@echo off
set "FalloutNVPath=E:\SteamLibrary\steamapps\common\Fallout New Vegas"
cmake -S "%~dp0.." -B "%~dp0..\build" -A Win32
if errorlevel 1 exit /b 1
cmake --build "%~dp0..\build" --config Release
if errorlevel 1 exit /b 1
echo.
echo Deploying...
copy /Y "%~dp0..\build\bin\Release\SkysFalloutCheats.dll" "%FalloutNVPath%\Data\NVSE\Plugins\SkysFalloutCheats.dll"
xcopy /E /I /Y "%~dp0..\data_package\*" "%FalloutNVPath%\Data\NVSE\Plugins\SkysFalloutCheats\"
echo Done.
