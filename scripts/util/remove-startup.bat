@echo off
echo Removing HDD Toggle from elevated startup...

:: Remove the scheduled task (check both old and new names for compatibility)
echo Removing scheduled task...
schtasks /delete /tn "HDD Toggle Startup" /f 2>nul
schtasks /delete /tn "HDD Control Startup" /f 2>nul

:: Check if either task still exists
schtasks /query /tn "HDD Toggle Startup" >nul 2>nul
set toggle_exists=%errorlevel%
schtasks /query /tn "HDD Control Startup" >nul 2>nul
set control_exists=%errorlevel%

if %toggle_exists% neq 0 if %control_exists% neq 0 (
    echo.
    echo ✓ SUCCESS! HDD Toggle startup entry has been removed.
    echo The application will no longer start automatically on login.
) else (
    echo.
    echo ✗ Failed to remove task. You may need to run as Administrator.
)

echo.
pause