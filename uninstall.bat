@echo off
setlocal

:: HDD Toggle Uninstaller

echo ============================================
echo  HDD Toggle Uninstaller
echo ============================================
echo.

set "INSTALL_DIR=%LOCALAPPDATA%\HDD-Toggle"
set "SHORTCUT_PATH=%APPDATA%\Microsoft\Windows\Start Menu\Programs\HDD Toggle.lnk"

:: Check if installed
if not exist "%INSTALL_DIR%\hdd-toggle.exe" (
    echo HDD Toggle does not appear to be installed at:
    echo %INSTALL_DIR%
    echo.
    pause
    exit /b 1
)

echo This will:
echo  1. Remove the scheduled startup task
echo  2. Delete the Start Menu shortcut
echo  3. Delete all files in %INSTALL_DIR%
echo.
echo Your config file will be deleted. Back it up if needed.
echo.
choice /c YN /m "Continue with uninstall"
if %errorlevel% neq 1 (
    echo Uninstall cancelled.
    pause
    exit /b 0
)
echo.

:: Remove scheduled task
echo Removing startup task...
schtasks /delete /tn "HDD Toggle" /f >nul 2>&1
if %errorlevel% equ 0 (
    echo Startup task removed.
) else (
    echo No startup task found (or already removed).
)

:: Also try the old task name
schtasks /delete /tn "HDD Control Startup" /f >nul 2>&1

:: Remove Start Menu shortcut
echo Removing Start Menu shortcut...
if exist "%SHORTCUT_PATH%" (
    del "%SHORTCUT_PATH%" >nul 2>&1
    echo Shortcut removed.
) else (
    echo No shortcut found.
)

:: Kill the process if running
echo Stopping HDD Toggle if running...
taskkill /f /im hdd-toggle.exe >nul 2>&1
:: Also try old executable name
taskkill /f /im hdd-control.exe >nul 2>&1

:: Small delay to ensure process is stopped
timeout /t 1 /nobreak >nul

:: Remove install directory
echo Removing installed files...
if exist "%INSTALL_DIR%" (
    rmdir /s /q "%INSTALL_DIR%" >nul 2>&1
    if exist "%INSTALL_DIR%" (
        echo WARNING: Could not fully remove install directory.
        echo Some files may be in use. Try again after reboot.
    ) else (
        echo Files removed.
    )
)

echo.
echo ============================================
echo  Uninstall Complete
echo ============================================
echo.
pause
