@echo off

REM Check for 'small' parameter
set "BUILD_TYPE=static"
set "OUTPUT_NAME=relay.exe"
if "%1"=="small" (
    set "BUILD_TYPE=dynamic"
    set "OUTPUT_NAME=relay-small.exe"
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
    cl.exe /nologo /O1 /Os /MD relay.c /Fe:%OUTPUT_NAME% hid.lib setupapi.lib /link /OPT:REF /OPT:ICF
) else (
    echo Compiling STATIC version ^(standalone, no dependencies^)...
    cl.exe /nologo /O2 /MT relay.c /Fe:%OUTPUT_NAME% hid.lib setupapi.lib
)
if exist relay.obj del relay.obj >nul 2>nul

REM Build eject.exe
echo.
echo Building eject utility...
if "%BUILD_TYPE%"=="dynamic" (
    cl.exe /nologo /O1 /Os /MD eject.c /Fe:eject.exe cfgmgr32.lib /link /OPT:REF /OPT:ICF
) else (
    cl.exe /nologo /O2 /MT eject.c /Fe:eject.exe cfgmgr32.lib
)
if exist eject.obj del eject.obj >nul 2>nul

if %errorlevel% equ 0 (
    echo.
    echo SUCCESS! Built utilities:
    for %%f in (%OUTPUT_NAME% eject.exe) do if exist %%f echo   %%f - %%~zf bytes
    if "%BUILD_TYPE%"=="dynamic" (
        echo.
        echo NOTE: eject.exe/relay-small.exe require Visual C++ Redistributable 2015-2022
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
echo   compile.bat         - Creates relay.exe and eject.exe (standalone)
echo   compile.bat small   - Creates relay-small.exe and eject.exe (needs VC++ redist)