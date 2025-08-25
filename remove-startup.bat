@echo off
echo Removing HDD Control from elevated startup...

:: Remove the scheduled task
echo Removing scheduled task...
schtasks /delete /tn "HDD Control Startup" /f

if %errorlevel% equ 0 (
    echo.
    echo ✓ SUCCESS! HDD Control startup entry has been removed.
    echo The application will no longer start automatically on login.
) else (
    echo.
    echo ✗ Task not found or failed to remove. Error code: %errorlevel%
    echo The task may not have been created or was already removed.
)

echo.
pause