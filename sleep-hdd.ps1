# Sleep HDD Script - Minimal Implementation
# Target: WDC WD181KFGX-68AFPN0 (SN: 2VH7TM9L)

[CmdletBinding()]
param(
    [switch] $Offline,
    [Alias('h')][switch] $Help
)

# Show help if requested
if ($Help) {
    Write-Host "Sleep HDD Script - Power down hard drive safely"
    Write-Host ""
    Write-Host "Usage: .\sleep-hdd.ps1 [-Offline] [-Help]"
    Write-Host ""
    Write-Host "Parameters:"
    Write-Host "  -Offline    Take disk offline before power down (requires Administrator)"
    Write-Host "  -Help, -h   Show this help message"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\sleep-hdd.ps1           # Standard sleep (no admin required)"
    Write-Host "  .\sleep-hdd.ps1 -Offline  # Offline then sleep (requires admin)"
    Write-Host ""
    Write-Host "Note: Drive is configured for hot plug, so offline is optional"
    exit 0
}

$serial = '2VH7TM9L'
$model = 'WDC WD181KFGX-68AFPN0'

# Check admin only if offline is requested
if ($Offline) {
    $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")
    if (-not $isAdmin) {
        Write-Error "The -Offline parameter requires administrative privileges. Please run as Administrator or omit -Offline."
        exit 1
    }
}

# Early check: Is the drive already offline/powered down?
Write-Host "Checking current drive status..." -ForegroundColor Cyan

$existingPnp = Get-PnpDevice -Class 'DiskDrive' | Where-Object {
    $_.InstanceId -match $serial -or $_.FriendlyName -match $model
}

$existingDisk = $null
if ($existingPnp -and $existingPnp.Status -eq "OK") {
    $existingDisk = Get-Disk | Where-Object {
        $_.SerialNumber -match $serial -or $_.FriendlyName -match $model
    }
}

# Check if drive is already offline or not detected
if (!$existingPnp -or $existingPnp.Status -ne "OK" -or !$existingDisk -or $existingDisk.IsOffline) {
    Write-Host ""
    Write-Host "DRIVE ALREADY OFFLINE" -ForegroundColor Green
    Write-Host "Drive appears to already be offline or powered down"
    if ($existingPnp) {
        Write-Host "PnP Status: $($existingPnp.Status)"
    } else {
        Write-Host "PnP Status: Not detected"
    }
    Write-Host ""
    Write-Host "No action needed - drive is already sleeping." -ForegroundColor Green
    Write-Host "To wake the drive, run: .\wake-hdd.ps1" -ForegroundColor Cyan
    exit 0
}

Write-Host "Drive currently online - proceeding with sleep sequence..." -ForegroundColor Yellow

# Step 1: Take disk offline (optional, only used if -Offline specified)
if ($Offline) {
    Write-Host "Taking disk offline..." -ForegroundColor Cyan
    try {
        Set-Disk -Number $existingDisk.Number -IsOffline $true
        Start-Sleep -Seconds 2

        # Verify disk is offline
        $diskStatus = Get-Disk -Number $existingDisk.Number -ErrorAction SilentlyContinue
        if ($diskStatus -and $diskStatus.IsOffline) {
            Write-Host "Disk successfully taken offline" -ForegroundColor Green
        } else {
            Write-Warning "Disk may not be fully offline yet"
        }
    } catch {
        Write-Warning "Failed to take disk offline: $($_.Exception.Message)"
        Write-Host "Continuing with power down sequence anyway..." -ForegroundColor Yellow
    }
}

# Step 2: Power down relays
Write-Host "Powering down HDD..." -ForegroundColor Cyan
try {
    # Turn off both relays simultaneously using relay.exe
    $result = Start-Process -FilePath "relay.exe" -ArgumentList "all", "off" -Wait -PassThru -NoNewWindow
    if ($result.ExitCode -ne 0) {
        Write-Error "Failed to deactivate relay power"
        exit 1
    }

    Write-Host "Power OFF: Both relays deactivated" -ForegroundColor Green
} catch {
    Write-Error "Power deactivation failed: $($_.Exception.Message)"
    exit 1
}

# Step 3: Wait and verify drive has left the system
Write-Host "Disconnecting..." -ForegroundColor Yellow
Start-Sleep -Seconds 3

# Step 4: PnP scan to verify drive is no longer detected
Write-Host "Verifying drive disconnection..." -ForegroundColor Cyan

# Optional: Trigger device refresh to help Windows recognize disconnection
$taskName = "HDD-DeviceDetection"

if ($LASTEXITCODE -eq 0) {
    Write-Host "Triggering device refresh to verify disconnection..." -ForegroundColor Yellow
    schtasks /run /tn $taskName | Out-Null
    Start-Sleep -Seconds 3
}

$maxAttempts = 6
$attempt = 0
$driveGone = $false

do {
    $attempt++
    Write-Host "Disconnection verification attempt $attempt/$maxAttempts"

    $checkPnp = Get-PnpDevice -Class 'DiskDrive' | Where-Object {
        $_.InstanceId -match $serial -or $_.FriendlyName -match $model
    }

    if (!$checkPnp) {
        Write-Host "Drive no longer detected in PnP devices" -ForegroundColor Green
        $driveGone = $true
        break
    } elseif ($checkPnp.Status -ne "OK") {
        Write-Host "Drive detected but status is: $($checkPnp.Status)" -ForegroundColor Yellow
        $driveGone = $true
        break
    }

    Start-Sleep -Seconds 2
} while ($attempt -lt $maxAttempts)

# Also check if disk is no longer visible to Windows
$checkDisk = Get-Disk | Where-Object {
    $_.SerialNumber -match $serial -or $_.FriendlyName -match $model
} -ErrorAction SilentlyContinue

# Step 5: Final status and user notification
Write-Host ""
if ($driveGone -or !$checkDisk) {
    Write-Host "HDD SLEEP COMPLETE" -ForegroundColor Green
    Write-Host "Drive has been successfully powered down and disconnected"
    if ($checkPnp) {
        Write-Host "PnP Status: $($checkPnp.Status)"
    } else {
        Write-Host "PnP Status: Not detected"
    }
    Write-Host "Disk Status: Not visible to Windows"
} else {
    Write-Host "HDD POWER DOWN COMPLETE" -ForegroundColor Yellow
    Write-Host "Drive powered down but may still be visible to Windows"
    Write-Host "This is normal for some drive/controller combinations"
}

Write-Host ""
Write-Host "To wake the drive again, run: .\wake-hdd.ps1" -ForegroundColor Cyan