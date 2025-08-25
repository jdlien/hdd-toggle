@echo off
echo Creating minimal HDD icon...

REM Create a simple 32x32 HDD icon using PowerShell
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "^
Add-Type -AssemblyName System.Drawing; ^
$bmp = New-Object System.Drawing.Bitmap(32, 32); ^
$g = [System.Drawing.Graphics]::FromImage($bmp); ^
$g.Clear([System.Drawing.Color]::Transparent); ^
$brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(64, 64, 64)); ^
$g.FillRectangle($brush, 4, 8, 24, 16); ^
$brush2 = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(128, 128, 128)); ^
$g.FillRectangle($brush2, 6, 10, 20, 12); ^
$brush3 = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(200, 200, 200)); ^
$g.FillRectangle($brush3, 8, 12, 16, 8); ^
$brush4 = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(32, 32, 32)); ^
$g.FillRectangle($brush4, 12, 14, 8, 4); ^
$g.Dispose(); ^
$icon = [System.Drawing.Icon]::FromHandle($bmp.GetHicon()); ^
$fs = New-Object System.IO.FileStream('hdd-icon.ico', [System.IO.FileMode]::Create); ^
$icon.Save($fs); ^
$fs.Close(); ^
$bmp.Dispose()"

if exist hdd-icon.ico (
    echo HDD icon created successfully: hdd-icon.ico
) else (
    echo Failed to create icon file
    exit /b 1
)