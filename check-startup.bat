@echo off
echo Checking HDD Control startup status...
echo.

:: Check if the scheduled task exists
schtasks /query /tn "HDD Control Startup" >nul 2>nul

if %errorlevel% equ 0 (
    echo ✓ HDD Control IS configured for elevated startup
    echo.
    echo Task details:
    schtasks /query /tn "HDD Control Startup" /fo list
) else (
    echo ✗ HDD Control is NOT configured for startup
    echo.
    echo To enable elevated startup, run: setup-startup.bat
)

echo.
pause