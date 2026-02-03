@echo off
echo Checking HDD Toggle startup status...
echo.

:: Check if the scheduled task exists
schtasks /query /tn "HDD Toggle Startup" >nul 2>nul

if %errorlevel% equ 0 (
    echo ✓ HDD Toggle IS configured for elevated startup
    echo.
    echo Task details:
    schtasks /query /tn "HDD Toggle Startup" /fo list
) else (
    echo ✗ HDD Toggle is NOT configured for startup
    echo.
    echo To enable elevated startup, run: setup-startup.bat
)

echo.
pause