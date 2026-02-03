@echo off
REM Build and run unit tests
REM Run from project root or from scripts/build/

REM Get script directory and project root
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%..\.."

REM Change to project root for consistent paths
pushd "%PROJECT_ROOT%"

echo Building HDD Control Tests...

set "VSPATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "VCPATH=%VSPATH%\VC\Tools\MSVC\14.44.35207"
set "SDKPATH=C:\Program Files (x86)\Windows Kits\10"
set "SDKVER=10.0.26100.0"

set "PATH=%VCPATH%\bin\Hostx64\x64;%SDKPATH%\bin\%SDKVER%\x64;%PATH%"
set "INCLUDE=%VCPATH%\include;%SDKPATH%\Include\%SDKVER%\ucrt;%SDKPATH%\Include\%SDKVER%\shared;%SDKPATH%\Include\%SDKVER%\um"
set "LIB=%VCPATH%\lib\x64;%SDKPATH%\Lib\%SDKVER%\ucrt\x64;%SDKPATH%\Lib\%SDKVER%\um\x64"

echo Compiling test runner (with debug symbols for coverage)...
cl.exe /nologo /EHsc /std:c++17 /Zi /I include /Fe:tests\run-tests.exe /Fd:tests\run-tests.pdb tests\test_main.cpp tests\test_utils.cpp

REM Clean up intermediate files (keep PDB for coverage)
if exist tests\test_main.obj del tests\test_main.obj >nul 2>nul
if exist tests\test_utils.obj del tests\test_utils.obj >nul 2>nul
if exist test_main.obj del test_main.obj >nul 2>nul
if exist test_utils.obj del test_utils.obj >nul 2>nul
if exist vc140.pdb del vc140.pdb >nul 2>nul

if %errorlevel% equ 0 (
    echo.
    echo SUCCESS! Built tests\run-tests.exe
    echo.
    echo Running tests...
    echo ========================================
    tests\run-tests.exe --reporter compact
    echo ========================================
) else (
    echo.
    echo Build failed with error code %errorlevel%
)

popd
