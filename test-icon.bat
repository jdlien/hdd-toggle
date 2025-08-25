@echo off
echo Testing icon file...

if exist hdd-icon.ico (
    echo Icon file exists
    for %%f in (hdd-icon.ico) do echo Size: %%~zf bytes
    
    REM Try to display basic info about the icon
    powershell -Command "Add-Type -AssemblyName System.Drawing; $icon = New-Object System.Drawing.Icon('hdd-icon.ico'); Write-Host 'Icon loaded successfully'; Write-Host \"Width: $($icon.Width)\"; Write-Host \"Height: $($icon.Height)\"; $icon.Dispose()"
) else (
    echo ERROR: hdd-icon.ico not found!
)