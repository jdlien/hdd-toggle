@echo off
echo Converting PNG to ICO with Windows 11 required sizes...

powershell.exe -NoProfile -ExecutionPolicy Bypass -File ConvertToIco.ps1

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS! Created hdd-icon.ico with all Windows 11 required sizes
    echo Sizes included: 16x16, 24x24, 32x32, 48x48, 64x64, 128x128, 256x256
    dir hdd-icon.ico 2>nul | findstr ".ico"
) else (
    echo.
    echo ERROR: Failed to convert PNG to ICO
    exit /b 1
)