@echo off
REM Build HDD Toggle unified binary
REM Run from project root or from scripts/build/
REM Usage: compile-gui.bat [x64|arm64]

REM Get script directory and project root
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%..\.."

REM Change to project root for consistent paths
pushd "%PROJECT_ROOT%"

REM Parse architecture argument (default to x64)
set "ARCH=x64"
if /i "%~1"=="arm64" set "ARCH=arm64"
if /i "%~1"=="x64" set "ARCH=x64"

echo Building HDD Toggle unified binary (%ARCH%)...
echo.

set "VSPATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "VCPATH=%VSPATH%\VC\Tools\MSVC\14.44.35207"
set "SDKPATH=C:\Program Files (x86)\Windows Kits\10"
set "SDKVER=10.0.26100.0"

if "%ARCH%"=="arm64" (
    set "PATH=%VCPATH%\bin\Hostx64\arm64;%SDKPATH%\bin\%SDKVER%\x64;%PATH%"
    set "INCLUDE=%VCPATH%\include;%SDKPATH%\Include\%SDKVER%\ucrt;%SDKPATH%\Include\%SDKVER%\shared;%SDKPATH%\Include\%SDKVER%\um;%SDKPATH%\Include\%SDKVER%\cppwinrt"
    set "LIB=%VCPATH%\lib\arm64;%SDKPATH%\Lib\%SDKVER%\ucrt\arm64;%SDKPATH%\Lib\%SDKVER%\um\arm64"
    set "OUTPUT=bin\hdd-toggle-arm64.exe"
) else (
    set "PATH=%VCPATH%\bin\Hostx64\x64;%SDKPATH%\bin\%SDKVER%\x64;%PATH%"
    set "INCLUDE=%VCPATH%\include;%SDKPATH%\Include\%SDKVER%\ucrt;%SDKPATH%\Include\%SDKVER%\shared;%SDKPATH%\Include\%SDKVER%\um;%SDKPATH%\Include\%SDKVER%\cppwinrt"
    set "LIB=%VCPATH%\lib\x64;%SDKPATH%\Lib\%SDKVER%\ucrt\x64;%SDKPATH%\Lib\%SDKVER%\um\x64"
    set "OUTPUT=bin\hdd-toggle.exe"
)

echo Compiling resource file...
rc.exe /nologo /fo res\hdd-icon.res res\hdd-icon.rc

if %errorlevel% neq 0 (
    echo Failed to compile resource file
    popd
    exit /b 1
)

echo Compiling unified binary...
cl.exe /nologo /O2 /MT /EHsc /std:c++17 /I include ^
    src\main.cpp ^
    src\core\process.cpp ^
    src\core\admin.cpp ^
    src\core\disk.cpp ^
    src\commands\relay.cpp ^
    src\commands\wake.cpp ^
    src\commands\sleep.cpp ^
    src\commands\status.cpp ^
    src\gui\tray-app.cpp ^
    /Fe:%OUTPUT% ^
    res\hdd-icon.res ^
    shell32.lib advapi32.lib user32.lib comctl32.lib wbemuuid.lib ole32.lib oleaut32.lib setupapi.lib dwmapi.lib hid.lib WindowsApp.lib shlwapi.lib propsys.lib ^
    /link /SUBSYSTEM:WINDOWS

REM Clean up intermediate files
if exist src\main.obj del src\main.obj >nul 2>nul
if exist src\core\process.obj del src\core\process.obj >nul 2>nul
if exist src\core\admin.obj del src\core\admin.obj >nul 2>nul
if exist src\core\disk.obj del src\core\disk.obj >nul 2>nul
if exist src\commands\relay.obj del src\commands\relay.obj >nul 2>nul
if exist src\commands\wake.obj del src\commands\wake.obj >nul 2>nul
if exist src\commands\sleep.obj del src\commands\sleep.obj >nul 2>nul
if exist src\commands\status.obj del src\commands\status.obj >nul 2>nul
if exist src\gui\tray-app.obj del src\gui\tray-app.obj >nul 2>nul
if exist *.obj del *.obj >nul 2>nul
if exist res\hdd-icon.res del res\hdd-icon.res >nul 2>nul

if %errorlevel% equ 0 (
    echo.
    echo SUCCESS! Built %OUTPUT%
    if exist %OUTPUT% for %%f in (%OUTPUT%) do echo   %%f - %%~zf bytes
    echo.
    echo Usage:
    echo   hdd-toggle              Launch tray app
    echo   hdd-toggle wake         Wake the drive
    echo   hdd-toggle sleep        Sleep the drive
    echo   hdd-toggle status       Show drive status
    echo   hdd-toggle relay on/off Control relay
    echo   hdd-toggle --help       Show help
) else (
    echo.
    echo Build failed with error code %errorlevel%
)

popd
