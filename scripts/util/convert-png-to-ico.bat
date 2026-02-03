@echo off
echo Converting PNG to ICO with all Windows required sizes using ImageMagick...

set "MAGICK_PATH=C:\Program Files\ImageMagick-7.1.2-Q16-HDRI\magick.exe"

if not exist "%MAGICK_PATH%" (
    echo ERROR: ImageMagick not found at: %MAGICK_PATH%
    echo Please update the path or install ImageMagick
    exit /b 1
)

if not exist "hdd-icon-512px.png" (
    echo ERROR: Source file hdd-icon-512px.png not found
    exit /b 1
)

echo Creating ICO with optimal Windows sizes: 256, 128, 64, 48, 40, 32, 24, 20, 16...
"%MAGICK_PATH%" hdd-icon-512px.png -define icon:auto-resize=256,128,64,48,40,32,24,20,16 hdd-icon.ico

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS! Created hdd-icon.ico with all Windows required sizes
    echo Sizes included: 16x16, 20x20, 24x24, 32x32, 40x40, 48x48, 64x64, 128x128, 256x256
    dir hdd-icon.ico 2>nul | findstr ".ico"
) else (
    echo.
    echo ERROR: Failed to convert PNG to ICO with ImageMagick
    exit /b 1
)