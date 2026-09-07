@echo off
set "G=E:\SteamLibrary\steamapps\common\Fallout New Vegas"
echo === What we added (mod files) ===
dir /b "%G%\nvse_*.*" 2>nul
dir /b "%G%\Data\NVSE\Plugins\SkysFalloutCheats*" 2>nul
dir /b "%G%\Data\NVSE\Plugins\SkysFalloutCheats" 2>nul
echo.
echo === Did we touch FalloutNV.exe? ===
powershell -NoProfile -Command "$f=Get-Item '%G%\FalloutNV.exe'; Write-Output ('FalloutNV.exe size=' + $f.Length + ' modified=' + $f.LastWriteTime)"
echo.
echo === Latest GameState lines ===
powershell -NoProfile -Command "Select-String -Path '%G%\Data\NVSE\Plugins\SkysFalloutCheats\sfc.log' -Pattern 'GameState' | Select-Object -Last 15"
