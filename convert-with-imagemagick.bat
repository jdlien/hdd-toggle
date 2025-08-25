@echo off
echo Converting PNG to ICO using ImageMagick...

REM Check if ImageMagick is available
where magick >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ImageMagick not found in PATH. Please ensure it's installed and in PATH.
    exit /b 1
)

REM Check if source file exists
if not exist "hdd-icon-512px.png" (
    echo Error: hdd-icon-512px.png not found!
    exit /b 1
)

echo Creating multi-resolution ICO file with Windows 11 sizes...

REM Use ImageMagick to create ICO with all required sizes
REM Windows 11 needs: 16x16, 24x24, 32x32, 48x48, 64x64, 128x128, 256x256
magick convert hdd-icon-512px.png ^
    -define icon:auto-resize="256,128,64,48,32,24,16" ^
    -compress none ^
    hdd-icon.ico

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS! Created hdd-icon.ico with ImageMagick
    echo Sizes included: 16x16, 24x24, 32x32, 48x48, 64x64, 128x128, 256x256
    
    REM Show file info
    for %%f in (hdd-icon.ico) do echo File created: %%~nxf - %%~zf bytes
    
    REM Verify with ImageMagick identify
    echo.
    echo Icon details:
    magick identify hdd-icon.ico
) else (
    echo.
    echo ERROR: Failed to convert PNG to ICO with ImageMagick
    exit /b 1
)