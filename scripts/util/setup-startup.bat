@echo off
echo Setting up HDD Toggle for elevated startup...

:: Get the current directory where the executable is located
set "INSTALL_DIR=%~dp0"
set "EXE_PATH=%INSTALL_DIR%hdd-toggle.exe"

echo Installation directory: %INSTALL_DIR%
echo Executable path: %EXE_PATH%

:: Check if the executable exists
if not exist "%EXE_PATH%" (
    echo ERROR: hdd-toggle.exe not found in current directory!
    echo Please run this script from the directory containing hdd-toggle.exe
    pause
    exit /b 1
)

echo.
echo This will create a Windows Task Scheduler entry to run HDD Toggle
echo with administrator privileges at system startup without UAC prompts.
echo.
echo Press any key to continue or Ctrl+C to cancel...
pause >nul

:: Create the scheduled task
echo Creating scheduled task...
schtasks /create /tn "HDD Toggle Startup" /tr "%EXE_PATH%" /sc onlogon /rl highest /f

if %errorlevel% equ 0 (
    echo.
    echo SUCCESS! HDD Toggle is now configured to start elevated on login.
    echo.
    echo The task will run with highest privileges without showing UAC prompts.
    echo You can verify this in Task Scheduler under "HDD Toggle Startup"
    echo.
    echo To remove this startup entry later, run: remove-startup.bat
) else (
    echo.
    echo FAILED to create scheduled task. Error code: %errorlevel%
    echo Make sure you're running this script as Administrator.
)

echo.
pause
