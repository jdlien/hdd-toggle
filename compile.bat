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

if "%BUILD_TYPE%"=="dynamic" (
    echo Compiling SMALL version with dynamic CRT ^(requires VC++ redistributable^)...
    cl.exe /nologo /O1 /Os /MD relay.c /Fe:%OUTPUT_NAME% hid.lib setupapi.lib /link /OPT:REF /OPT:ICF
) else (
    echo Compiling STATIC version ^(standalone, no dependencies^)...
    cl.exe /nologo /O2 /MT relay.c /Fe:%OUTPUT_NAME% hid.lib setupapi.lib
)

if %errorlevel% equ 0 (
    echo.
    echo SUCCESS! %OUTPUT_NAME% created
    if exist relay.obj del relay.obj

    echo.
    for %%f in (%OUTPUT_NAME%) do echo File size: %%~zf bytes

    if "%BUILD_TYPE%"=="dynamic" (
        echo.
        echo NOTE: This version requires Visual C++ Redistributable 2015-2022
        echo       Most Windows systems already have this installed.
    ) else (
        echo.
        echo This is a standalone executable with no dependencies.
    )
) else (
    echo Compilation failed!
)

echo.
echo Usage:
echo   compile.bat         - Creates relay.exe (140KB, standalone)
echo   compile.bat small   - Creates relay-small.exe (12KB, needs VC++ redist)