# Device Detection with UAC Prompt
# This will prompt for admin credentials when run as regular user

Write-Host "Device Detection - Elevation Required" -ForegroundColor Cyan
Write-Host "This will prompt for administrator credentials..." -ForegroundColor Yellow

try {
    # Create a script block with the detection commands
    $scriptBlock = {
        Write-Host "Running device detection as administrator..." -ForegroundColor Green
        
        # PnP device scan
        Write-Host "Scanning for PnP devices..."
        pnputil /scan-devices | Out-Host
        
        Start-Sleep -Seconds 2
        
        # Disk rescan
        Write-Host "Rescanning disks..."
        "rescan" | diskpart | Out-Host
        
        Write-Host "Device detection completed" -ForegroundColor Green
        
        # Keep window open briefly to show results
        Start-Sleep -Seconds 2
    }
    
    # Convert script block to string and encode for passing to elevated PowerShell
    $encodedCommand = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($scriptBlock.ToString()))
    
    # Start elevated PowerShell process
    $proc = Start-Process -FilePath "powershell.exe" -ArgumentList "-EncodedCommand", $encodedCommand -Verb RunAs -Wait -PassThru
    
    if ($proc.ExitCode -eq 0) {
        Write-Host "Device detection completed successfully" -ForegroundColor Green
        return $true
    } else {
        Write-Warning "Device detection may have failed (exit code: $($proc.ExitCode))"
        return $false
    }
    
} catch {
    Write-Error "Failed to run elevated device detection: $($_.Exception.Message)"
    return $false
}