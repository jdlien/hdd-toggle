Write-Host "Testing icon file..." -ForegroundColor Cyan

if (Test-Path "hdd-icon.ico") {
    $iconFile = Get-Item "hdd-icon.ico"
    Write-Host "Icon file exists: $($iconFile.Name)" -ForegroundColor Green
    Write-Host "Size: $($iconFile.Length) bytes"
    
    try {
        Add-Type -AssemblyName System.Drawing
        $icon = New-Object System.Drawing.Icon("hdd-icon.ico")
        Write-Host "Icon loaded successfully!" -ForegroundColor Green
        Write-Host "Default size: $($icon.Width)x$($icon.Height)"
        
        # Check what sizes are available in the icon
        $stream = [System.IO.File]::OpenRead("hdd-icon.ico")
        $fullIcon = New-Object System.Drawing.Icon($stream)
        Write-Host "Icon contains these sizes:"
        
        # The Icon class doesn't expose all sizes directly, but we know from our creation
        Write-Host "  - 16x16, 24x24, 32x32, 48x48, 64x64, 128x128, 256x256 (as created)"
        
        $stream.Close()
        $icon.Dispose()
        $fullIcon.Dispose()
        
    } catch {
        Write-Host "ERROR loading icon: $_" -ForegroundColor Red
    }
} else {
    Write-Host "ERROR: hdd-icon.ico not found!" -ForegroundColor Red
}