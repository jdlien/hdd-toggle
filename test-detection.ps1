# Test Drive Detection Methods
# Tries various non-admin methods to trigger Windows drive detection

$serial = '2VH7TM9L'
$model = 'WDC WD181KFGX-68AFPN0'

function Test-DriveDetected {
    Write-Host "`nChecking for drive..." -ForegroundColor Cyan
    
    $pnp = Get-PnpDevice -Class 'DiskDrive' | Where-Object { 
        $_.InstanceId -match $serial -or $_.FriendlyName -match $model 
    }
    
    if ($pnp) {
        Write-Host "✓ PnP Device Found: $($pnp.FriendlyName) (Status: $($pnp.Status))" -ForegroundColor Green
        
        $disk = Get-Disk | Where-Object { 
            $_.SerialNumber -match $serial -or $_.FriendlyName -match $model 
        } -ErrorAction SilentlyContinue
        
        if ($disk) {
            Write-Host "✓ Disk Found: $($disk.FriendlyName) (Number: $($disk.Number), Offline: $($disk.IsOffline))" -ForegroundColor Green
            return $true
        } else {
            Write-Host "⚠ PnP device found but not visible as disk yet" -ForegroundColor Yellow
            return $false
        }
    } else {
        Write-Host "✗ Drive not detected" -ForegroundColor Red
        return $false
    }
}

Write-Host "Drive Detection Test Suite" -ForegroundColor Yellow
Write-Host "=========================" -ForegroundColor Yellow

# Initial check
Write-Host "`n1. Initial Status Check" -ForegroundColor Cyan
$detected = Test-DriveDetected
if ($detected) { 
    Write-Host "Drive already detected - test complete!" -ForegroundColor Green
    exit 0 
}

# Method 1: PowerShell device cache refresh
Write-Host "`n2. Method 1: PowerShell Device Cache Refresh" -ForegroundColor Cyan
Write-Host "Forcing Get-PnpDevice to refresh cache..."
Get-PnpDevice -Class 'DiskDrive' | Out-Null
Start-Sleep -Seconds 2
$detected = Test-DriveDetected
if ($detected) { 
    Write-Host "SUCCESS: PowerShell cache refresh worked!" -ForegroundColor Green
    exit 0 
}

# Method 2: WMI enumeration
Write-Host "`n3. Method 2: WMI Hardware Enumeration" -ForegroundColor Cyan
Write-Host "Refreshing WMI hardware enumeration..."
Get-WmiObject Win32_DiskDrive | Out-Null
Get-WmiObject Win32_LogicalDisk | Out-Null
Start-Sleep -Seconds 2
$detected = Test-DriveDetected
if ($detected) { 
    Write-Host "SUCCESS: WMI enumeration worked!" -ForegroundColor Green
    exit 0 
}

# Method 3: CIM instance refresh
Write-Host "`n4. Method 3: CIM Instance Refresh" -ForegroundColor Cyan
Write-Host "Refreshing CIM instances..."
Get-CimInstance Win32_DiskDrive | Out-Null
Get-CimInstance Win32_LogicalDisk | Out-Null
Start-Sleep -Seconds 2
$detected = Test-DriveDetected
if ($detected) { 
    Write-Host "SUCCESS: CIM refresh worked!" -ForegroundColor Green
    exit 0 
}

# Method 4: Registry key access
Write-Host "`n5. Method 4: Registry Key Access" -ForegroundColor Cyan
Write-Host "Accessing storage-related registry keys..."
Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Services\disk\Enum" -ErrorAction SilentlyContinue | Out-Null
Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Services\usbstor\Enum" -ErrorAction SilentlyContinue | Out-Null
Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Services\storahci\Enum" -ErrorAction SilentlyContinue | Out-Null
Start-Sleep -Seconds 2
$detected = Test-DriveDetected
if ($detected) { 
    Write-Host "SUCCESS: Registry access worked!" -ForegroundColor Green
    exit 0 
}

# Method 5: Volume and partition refresh
Write-Host "`n6. Method 5: Volume and Partition Refresh" -ForegroundColor Cyan
Write-Host "Refreshing volume and partition enumeration..."
Get-Volume | Out-Null
Get-Partition | Out-Null -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
$detected = Test-DriveDetected
if ($detected) { 
    Write-Host "SUCCESS: Volume refresh worked!" -ForegroundColor Green
    exit 0 
}

# Method 6: Physical disk enumeration
Write-Host "`n7. Method 6: Physical Disk Enumeration" -ForegroundColor Cyan
Write-Host "Enumerating physical disks..."
Get-PhysicalDisk | Out-Null -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
$detected = Test-DriveDetected
if ($detected) { 
    Write-Host "SUCCESS: Physical disk enumeration worked!" -ForegroundColor Green
    exit 0 
}

# Method 7: Storage subsystem refresh
Write-Host "`n8. Method 7: Storage Subsystem Refresh" -ForegroundColor Cyan
Write-Host "Refreshing storage subsystem..."
Get-StorageSubsystem | Out-Null -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
$detected = Test-DriveDetected
if ($detected) { 
    Write-Host "SUCCESS: Storage subsystem refresh worked!" -ForegroundColor Green
    exit 0 
}

# Method 8: Force hardware re-enumeration via WMI
Write-Host "`n9. Method 8: WMI Hardware Re-enumeration" -ForegroundColor Cyan
Write-Host "Triggering WMI hardware re-scan..."
try {
    $computer = Get-WmiObject Win32_ComputerSystem
    $computer.PSBase | Out-Null
} catch { }
Start-Sleep -Seconds 2
$detected = Test-DriveDetected
if ($detected) { 
    Write-Host "SUCCESS: WMI hardware re-scan worked!" -ForegroundColor Green
    exit 0 
}

# Method 9: Combined approach with longer wait
Write-Host "`n10. Method 9: Combined Approach with Extended Wait" -ForegroundColor Cyan
Write-Host "Running all methods together with longer wait..."
Get-PnpDevice -Class 'DiskDrive' | Out-Null
Get-WmiObject Win32_DiskDrive | Out-Null
Get-CimInstance Win32_DiskDrive | Out-Null
Get-Volume | Out-Null
Write-Host "Waiting 5 seconds for detection..."
Start-Sleep -Seconds 5
$detected = Test-DriveDetected
if ($detected) { 
    Write-Host "SUCCESS: Combined approach worked!" -ForegroundColor Green
    exit 0 
}

# Final status
Write-Host "`nAll detection methods failed." -ForegroundColor Red
Write-Host "The drive may need:" -ForegroundColor Yellow
Write-Host "  - More time to spin up and initialize" 
Write-Host "  - Administrative privileges for device enablement"
Write-Host "  - Manual intervention in Device Manager"
Write-Host "  - Hardware troubleshooting"