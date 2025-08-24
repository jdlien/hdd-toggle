@echo off
echo Compiling USB Relay Controller...

REM Check if Visual Studio Build Tools are available
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Microsoft C compiler (cl.exe) not found in PATH
    echo.
    echo Please install one of the following:
    echo 1. Visual Studio 2019/2022 with C++ build tools
    echo 2. Visual Studio Build Tools (standalone)
    echo 3. Run this from a "Developer Command Prompt"
    echo.
    echo Alternatively, try: gcc -o relay.exe relay.c -lhid -lsetupapi
    pause
    exit /b 1
)

REM Compile with Microsoft Visual C++
cl /nologo relay.c /Fe:relay.exe

if %errorlevel% equ 0 (
    echo.
    echo Compilation successful! relay.exe created.
    echo Usage: relay.exe {1^|2^|all} {on^|off}
    echo Example: relay.exe all on
) else (
    echo Compilation failed!
)

REM Clean up intermediate files
if exist relay.obj del relay.obj

pause