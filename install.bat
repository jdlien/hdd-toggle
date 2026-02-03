@echo off
setlocal enabledelayedexpansion

:: HDD Toggle Installer
:: Installs to %LOCALAPPDATA%\HDD-Toggle and sets up Start Menu shortcut + startup task

echo ============================================
echo  HDD Toggle Installer
echo ============================================
echo.

:: Determine source directory (where this script is located)
set "SOURCE_DIR=%~dp0"
set "SOURCE_DIR=%SOURCE_DIR:~0,-1%"

:: Default install location
set "INSTALL_DIR=%LOCALAPPDATA%\HDD-Toggle"

:: Check if we're running from source (has bin folder) or from a release package
if exist "%SOURCE_DIR%\bin\hdd-control.exe" (
    set "BIN_SOURCE=%SOURCE_DIR%\bin"
) else if exist "%SOURCE_DIR%\hdd-control.exe" (
    set "BIN_SOURCE=%SOURCE_DIR%"
) else (
    echo ERROR: Cannot find hdd-control.exe
    echo Run this script from the project root or a release package.
    pause
    exit /b 1
)

echo Source: %BIN_SOURCE%
echo Install to: %INSTALL_DIR%
echo.

:: Check for required executables
set "MISSING="
if not exist "%BIN_SOURCE%\hdd-control.exe" set "MISSING=!MISSING! hdd-control.exe"
if not exist "%BIN_SOURCE%\relay.exe" set "MISSING=!MISSING! relay.exe"
if not exist "%BIN_SOURCE%\wake-hdd.exe" set "MISSING=!MISSING! wake-hdd.exe"
if not exist "%BIN_SOURCE%\sleep-hdd.exe" set "MISSING=!MISSING! sleep-hdd.exe"

if defined MISSING (
    echo ERROR: Missing required files:%MISSING%
    echo Please build the project first: scripts\build\compile-gui.bat
    pause
    exit /b 1
)

:: Check for RemoveDrive.exe on PATH
where RemoveDrive.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo WARNING: RemoveDrive.exe not found on PATH
    echo The sleep function requires RemoveDrive.exe for safe drive ejection.
    echo Download from: https://www.uwe-sieber.de/drivetools_e.html
    echo.
)

echo This will:
echo  1. Copy files to %INSTALL_DIR%
echo  2. Create a Start Menu shortcut (required for notifications)
echo  3. Optionally add to startup (runs elevated without UAC prompts)
echo.
echo Press any key to continue or Ctrl+C to cancel...
pause >nul
echo.

:: Create install directory
if not exist "%INSTALL_DIR%" (
    echo Creating install directory...
    mkdir "%INSTALL_DIR%"
    if %errorlevel% neq 0 (
        echo ERROR: Failed to create install directory
        pause
        exit /b 1
    )
)

:: Copy executables
echo Copying executables...
copy /y "%BIN_SOURCE%\hdd-control.exe" "%INSTALL_DIR%\" >nul
copy /y "%BIN_SOURCE%\relay.exe" "%INSTALL_DIR%\" >nul
copy /y "%BIN_SOURCE%\wake-hdd.exe" "%INSTALL_DIR%\" >nul
copy /y "%BIN_SOURCE%\sleep-hdd.exe" "%INSTALL_DIR%\" >nul

:: Copy config file (don't overwrite existing user config)
if not exist "%INSTALL_DIR%\hdd-control.ini" (
    if exist "%SOURCE_DIR%\hdd-control.ini" (
        echo Copying user config...
        copy /y "%SOURCE_DIR%\hdd-control.ini" "%INSTALL_DIR%\" >nul
    ) else if exist "%SOURCE_DIR%\hdd-control.ini.example" (
        echo Copying example config (edit with your drive serial number)...
        copy /y "%SOURCE_DIR%\hdd-control.ini.example" "%INSTALL_DIR%\hdd-control.ini" >nul
    )
) else (
    echo Keeping existing config file.
)

:: Copy icon for shortcut (if available)
if exist "%SOURCE_DIR%\assets\icons\hdd-icon.ico" (
    copy /y "%SOURCE_DIR%\assets\icons\hdd-icon.ico" "%INSTALL_DIR%\" >nul
)

echo Files copied successfully.
echo.

:: Create Start Menu shortcut with AUMID (required for toast notifications)
echo Creating Start Menu shortcut...
set "SHORTCUT_PATH=%APPDATA%\Microsoft\Windows\Start Menu\Programs\HDD Toggle.lnk"
set "AUMID=JDLien.HDDToggle"

:: Use PowerShell to create shortcut with AppUserModelId
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ws = New-Object -ComObject WScript.Shell; ^
    $shortcut = $ws.CreateShortcut('%SHORTCUT_PATH%'); ^
    $shortcut.TargetPath = '%INSTALL_DIR%\hdd-control.exe'; ^
    $shortcut.WorkingDirectory = '%INSTALL_DIR%'; ^
    $shortcut.Description = 'HDD Toggle - Control mechanical hard drive power'; ^
    if (Test-Path '%INSTALL_DIR%\hdd-icon.ico') { $shortcut.IconLocation = '%INSTALL_DIR%\hdd-icon.ico' }; ^
    $shortcut.Save(); ^
    $shell = New-Object -ComObject Shell.Application; ^
    $folder = $shell.Namespace((Split-Path '%SHORTCUT_PATH%')); ^
    $item = $folder.ParseName((Split-Path '%SHORTCUT_PATH%' -Leaf));"

if %errorlevel% equ 0 (
    echo Start Menu shortcut created.
) else (
    echo WARNING: Failed to create Start Menu shortcut.
    echo Toast notifications may not work properly.
)
echo.

:: Ask about startup
echo Do you want HDD Toggle to start automatically on login?
echo (It will run with administrator privileges without UAC prompts)
echo.
choice /c YN /m "Add to startup"
if %errorlevel% equ 1 (
    echo.
    echo Creating startup task...
    schtasks /create /tn "HDD Toggle" /tr "\"%INSTALL_DIR%\hdd-control.exe\"" /sc onlogon /rl highest /f >nul 2>&1
    if %errorlevel% equ 0 (
        echo Startup task created.
    ) else (
        echo WARNING: Failed to create startup task.
        echo You may need to run this installer as Administrator.
    )
) else (
    echo Skipping startup configuration.
)

echo.
echo ============================================
echo  Installation Complete!
echo ============================================
echo.
echo Installed to: %INSTALL_DIR%
echo.
echo IMPORTANT: Edit %INSTALL_DIR%\hdd-control.ini
echo            to set your drive's serial number.
echo.
echo Find your drive serial with:
echo   wmic diskdrive get model,serialnumber
echo.
echo To uninstall, run: uninstall.bat
echo.
pause
