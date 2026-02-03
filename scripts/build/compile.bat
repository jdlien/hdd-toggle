@echo off
REM Build all C utilities (relay, wake-hdd, sleep-hdd)
REM Run from project root or from scripts/build/

REM Get script directory and project root
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%..\.."

REM Change to project root for consistent paths
pushd "%PROJECT_ROOT%"

REM Check for 'small' parameter
set "BUILD_TYPE=static"
set "OUTPUT_NAME=bin\relay.exe"
if "%1"=="small" (
    set "BUILD_TYPE=dynamic"
    set "OUTPUT_NAME=bin\relay-small.exe"
)

set "VSPATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "VCPATH=%VSPATH%\VC\Tools\MSVC\14.44.35207"
set "SDKPATH=C:\Program Files (x86)\Windows Kits\10"
set "SDKVER=10.0.26100.0"

set "PATH=%VCPATH%\bin\Hostx64\x64;%PATH%"
set "INCLUDE=%VCPATH%\include;%SDKPATH%\Include\%SDKVER%\ucrt;%SDKPATH%\Include\%SDKVER%\shared;%SDKPATH%\Include\%SDKVER%\um"
set "LIB=%VCPATH%\lib\x64;%SDKPATH%\Lib\%SDKVER%\ucrt\x64;%SDKPATH%\Lib\%SDKVER%\um\x64"

echo Building relay utility...
if "%BUILD_TYPE%"=="dynamic" (
    echo Compiling SMALL version with dynamic CRT ^(requires VC++ redistributable^)...
    cl.exe /nologo /O1 /Os /MD src\relay.c /Fe:%OUTPUT_NAME% hid.lib setupapi.lib /link /OPT:REF /OPT:ICF
) else (
    echo Compiling STATIC version ^(standalone, no dependencies^)...
    cl.exe /nologo /O2 /MT src\relay.c /Fe:%OUTPUT_NAME% hid.lib setupapi.lib
)
if exist src\relay.obj del src\relay.obj >nul 2>nul
if exist relay.obj del relay.obj >nul 2>nul

REM Build wake-hdd.exe
echo.
echo Building wake-hdd utility...
if "%BUILD_TYPE%"=="dynamic" (
    cl.exe /nologo /O1 /Os /MD src\wake-hdd.c /Fe:bin\wake-hdd.exe shell32.lib advapi32.lib /link /OPT:REF /OPT:ICF
) else (
    cl.exe /nologo /O2 /MT src\wake-hdd.c /Fe:bin\wake-hdd.exe shell32.lib advapi32.lib
)
if exist src\wake-hdd.obj del src\wake-hdd.obj >nul 2>nul
if exist wake-hdd.obj del wake-hdd.obj >nul 2>nul

REM Build sleep-hdd.exe
echo.
echo Building sleep-hdd utility...
if "%BUILD_TYPE%"=="dynamic" (
    cl.exe /nologo /O1 /Os /MD src\sleep-hdd.c /Fe:bin\sleep-hdd.exe shell32.lib advapi32.lib /link /OPT:REF /OPT:ICF
) else (
    cl.exe /nologo /O2 /MT src\sleep-hdd.c /Fe:bin\sleep-hdd.exe shell32.lib advapi32.lib
)
if exist src\sleep-hdd.obj del src\sleep-hdd.obj >nul 2>nul
if exist sleep-hdd.obj del sleep-hdd.obj >nul 2>nul

if %errorlevel% equ 0 (
    echo.
    echo SUCCESS! Built utilities:
    for %%f in (bin\relay.exe bin\wake-hdd.exe bin\sleep-hdd.exe) do if exist %%f echo   %%f - %%~zf bytes
    if "%BUILD_TYPE%"=="dynamic" (
        echo.
        echo NOTE: Binaries require Visual C++ Redistributable 2015-2022
    ) else (
        echo.
        echo Binaries are standalone with no dependencies.
    )
) else (
    echo.
    echo Build reported errors. Verify toolchain paths and try again.
)

echo.
echo Usage:
echo   compile.bat         - Creates standalone binaries (no dependencies)
echo   compile.bat small   - Creates smaller binaries (needs VC++ redist)

popd
