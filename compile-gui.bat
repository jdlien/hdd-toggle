@echo off
echo Building HDD Control GUI with icon...

set "VSPATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "VCPATH=%VSPATH%\VC\Tools\MSVC\14.44.35207"
set "SDKPATH=C:\Program Files (x86)\Windows Kits\10"
set "SDKVER=10.0.26100.0"

set "PATH=%VCPATH%\bin\Hostx64\x64;%SDKPATH%\bin\%SDKVER%\x64;%PATH%"
set "INCLUDE=%VCPATH%\include;%SDKPATH%\Include\%SDKVER%\ucrt;%SDKPATH%\Include\%SDKVER%\shared;%SDKPATH%\Include\%SDKVER%\um;%SDKPATH%\Include\%SDKVER%\cppwinrt"
set "LIB=%VCPATH%\lib\x64;%SDKPATH%\Lib\%SDKVER%\ucrt\x64;%SDKPATH%\Lib\%SDKVER%\um\x64"

echo Compiling resource file...
rc.exe /nologo /fo hdd-icon.res hdd-icon.rc

if %errorlevel% neq 0 (
    echo Failed to compile resource file
    exit /b 1
)

echo Compiling GUI application...
cl.exe /nologo /O2 /MT /EHsc /std:c++17 hdd-control-gui.cpp /Fe:hdd-control.exe hdd-icon.res shell32.lib advapi32.lib user32.lib comctl32.lib wbemuuid.lib ole32.lib oleaut32.lib setupapi.lib dwmapi.lib WindowsApp.lib /link /SUBSYSTEM:WINDOWS

if exist hdd-control-gui.obj del hdd-control-gui.obj >nul 2>nul
if exist hdd-icon.res del hdd-icon.res >nul 2>nul

if %errorlevel% equ 0 (
    echo SUCCESS! Built hdd-control.exe with icon
    if exist hdd-control.exe for %%f in (hdd-control.exe) do echo   hdd-control.exe - %%~zf bytes
) else (
    echo Build failed with error code %errorlevel%
)