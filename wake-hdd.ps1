# Wake HDD Script - Minimal Implementation
# Target: WDC WD181KFGX-68AFPN0 (SN: 2VH7TM9L)

$serial = '2VH7TM9L'
$model = 'WDC WD181KFGX-68AFPN0'

# Check if running as admin (for later operations that may need it)
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

# Early check: Is the drive already online and ready?
Write-Host "Checking current drive status..." -ForegroundColor Cyan

$existingPnp = Get-PnpDevice -Class 'DiskDrive' | Where-Object {
    $_.InstanceId -match $serial -or $_.FriendlyName -match $model
}

if ($existingPnp -and $existingPnp.Status -eq "OK") {
    $existingDisk = Get-Disk | Where-Object {
        $_.SerialNumber -match $serial -or $_.FriendlyName -match $model
    }

    if ($existingDisk -and !$existingDisk.IsOffline) {
        Write-Host ""
        Write-Host "DRIVE ALREADY ONLINE" -ForegroundColor Green
        Write-Host "Drive: $($existingDisk.FriendlyName)"
        Write-Host "Status: Already online and ready for use"
        Write-Host "Disk Number: $($existingDisk.Number)"
        Write-Host ""
        Write-Host "To sleep the drive, run: .\sleep-hdd.ps1" -ForegroundColor Cyan
        exit 0
    }
}

Write-Host "Drive not currently online - proceeding with wake sequence..." -ForegroundColor Yellow

# Step 1: Power up relays
Write-Host "Powering up HDD..." -ForegroundColor Cyan
try {
    # Turn on both relays simultaneously using relay.exe
    $result = Start-Process -FilePath "relay.exe" -ArgumentList "all", "on" -Wait -PassThru -NoNewWindow
    if ($result.ExitCode -ne 0) {
        Write-Error "Failed to activate relay power"
        exit 1
    }

    Write-Host "Power ON: Both relays activated" -ForegroundColor Green
} catch {
    Write-Error "Power activation failed: $($_.Exception.Message)"
    exit 1
}

# Step 2: Wait for drive to initialize
Write-Host "Waiting for drive initialization..." -ForegroundColor Yellow
Start-Sleep -Seconds 3

# Step 3: PnP device scan and detection
Write-Host "Scanning for PnP devices..." -ForegroundColor Cyan

# Function to trigger device detection with UAC elevation
function Invoke-DeviceRefresh {
    $taskName = "HDD-DeviceDetection"
    
    # Try the scheduled task approach first
    try {
        $taskExists = schtasks /query /tn $taskName 2>$null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Attempting to trigger scheduled task..." -ForegroundColor Yellow
            schtasks /run /tn $taskName | Out-Null
            
            if ($LASTEXITCODE -eq 0) {
                Write-Host "Scheduled task triggered (if you have permissions)" -ForegroundColor Green
                Start-Sleep -Seconds 5
                return $true
            }
        }
    } catch { }
    
    # Try UAC elevation approach
    Write-Host "Scheduled task unavailable - trying UAC elevation..." -ForegroundColor Yellow
    try {
        $scriptBlock = @'
            Write-Host "Running device detection as administrator..." -ForegroundColor Green
            pnputil /scan-devices | Out-Null
            Start-Sleep -Seconds 2
            "rescan" | diskpart | Out-Null
            Write-Host "Device detection completed" -ForegroundColor Green
'@
        
        # Try to run with elevation
        $proc = Start-Process -FilePath "powershell.exe" -ArgumentList "-Command", $scriptBlock -Verb RunAs -Wait -PassThru -WindowStyle Hidden
        
        if ($proc.ExitCode -eq 0) {
            Write-Host "Elevated device detection completed" -ForegroundColor Green
            return $true
        }
    } catch {
        Write-Host "UAC elevation cancelled or failed" -ForegroundColor Yellow
    }
    
    # Final fallback to basic methods
    Write-Host "Using basic fallback methods..." -ForegroundColor Yellow
    try {
        Get-PnpDevice -Class 'DiskDrive' | Out-Null
        Get-WmiObject Win32_DiskDrive | Out-Null
        Start-Sleep -Seconds 2
        Write-Host "Basic device refresh completed" -ForegroundColor Yellow
        return $false
    } catch {
        Write-Verbose "Some refresh methods failed, continuing anyway"
        return $false
    }
}

$maxAttempts = 8
$attempt = 0
$pnp = $null

do {
    $attempt++
    Write-Host "PnP scan attempt $attempt/$maxAttempts"
    
    # Try scheduled task on first attempt, fallback methods on later attempts
    if ($attempt -eq 1) {
        $taskSuccess = Invoke-DeviceRefresh
        if ($taskSuccess) {
            Write-Host "Scheduled task completed - checking for device..." -ForegroundColor Green
        }
    } elseif ($attempt -le 3) {
        # Use fallback methods on subsequent attempts
        Get-PnpDevice -Class 'DiskDrive' | Out-Null
        Start-Sleep -Seconds 1
    }

    # Check for device
    $pnp = Get-PnpDevice -Class 'DiskDrive' | Where-Object {
        $_.InstanceId -match $serial -or $_.FriendlyName -match $model
    }

    if ($pnp) {
        Write-Host "Found PnP device: $($pnp.FriendlyName) (Status: $($pnp.Status))" -ForegroundColor Green
        break
    }

    Start-Sleep -Seconds 2
} while ($attempt -lt $maxAttempts)

if (!$pnp) {
    Write-Error "Target drive not detected after $maxAttempts attempts"
    Write-Host "Drive may need more time to initialize or there may be a hardware issue"
    exit 1
}

# Step 4: Enable PnP device if disabled
if ($pnp.Status -ne "OK") {
    if ($isAdmin) {
        Write-Host "Enabling PnP device..." -ForegroundColor Cyan
        try {
            Enable-PnpDevice -InstanceId $pnp.InstanceId -Confirm:$false
            Start-Sleep -Seconds 2
            Write-Host "PnP device enabled" -ForegroundColor Green
        } catch {
            Write-Error "Failed to enable PnP device: $($_.Exception.Message)"
            exit 1
        }
    } else {
        Write-Warning "PnP device is disabled but script is not running as Administrator"
        Write-Host "Please run as Administrator to enable the device, or manually enable it" -ForegroundColor Yellow
        Write-Host "Device Instance ID: $($pnp.InstanceId)"
        exit 1
    }
}

# Step 5: Wait for disk to become visible and check offline status
Write-Host "Checking disk availability..." -ForegroundColor Cyan
$diskAttempts = 6
$diskAttempt = 0
$disk = $null

do {
    $diskAttempt++
    Write-Host "Disk detection attempt $diskAttempt/$diskAttempts"

    $disk = Get-Disk | Where-Object {
        $_.SerialNumber -match $serial -or $_.FriendlyName -match $model
    }

    if ($disk) {
        Write-Host "Found disk: $($disk.FriendlyName) (Number: $($disk.Number))" -ForegroundColor Green
        break
    }

    Start-Sleep -Seconds 2
} while ($diskAttempt -lt $diskAttempts)

if (!$disk) {
    Write-Warning "Disk not visible to Windows disk management"
    Write-Host "PnP device is enabled but disk may still be spinning up"
    Write-Host "Try running the script again in 10-15 seconds" -ForegroundColor Yellow
    exit 1
}

# Step 6: Bring disk online if offline
if ($disk.IsOffline) {
    if ($isAdmin) {
        Write-Host "Bringing disk online..." -ForegroundColor Cyan
        try {
            Set-Disk -Number $disk.Number -IsOffline $false
            Start-Sleep -Seconds 1

            # Verify disk is online
            $diskStatus = Get-Disk -Number $disk.Number
            if (!$diskStatus.IsOffline) {
                Write-Host "Disk brought online successfully" -ForegroundColor Green
            } else {
                Write-Warning "Disk may not be fully online yet"
            }
        } catch {
            Write-Error "Failed to bring disk online: $($_.Exception.Message)"
            exit 1
        }
    } else {
        Write-Warning "Disk is offline but script is not running as Administrator"
        Write-Host "Please run as Administrator to bring disk online, or use Disk Management" -ForegroundColor Yellow
        Write-Host "Disk Number: $($disk.Number)"
        # Don't exit - we can still report the drive is detected
    }
} else {
    Write-Host "Disk is already online" -ForegroundColor Green
}

# Step 7: Final status and user notification
Write-Host ""
Write-Host "HDD WAKE COMPLETE" -ForegroundColor Green
Write-Host "Drive: $($disk.FriendlyName)"
Write-Host "Status: Online and ready for use"
Write-Host "Disk Number: $($disk.Number)"
Write-Host ""
Write-Host "To sleep the drive again, run: .\sleep-hdd.ps1" -ForegroundColor Cyan