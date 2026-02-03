# Wake HDD Script - Simplified
# Target: WDC WD181KFGX-68AFPN0 (SN: 2VH7TM9L)

$serial = '2VH7TM9L'
$model = 'WDC WD181KFGX-68AFPN0'

# Check admin (only needed if we must bring the disk online)
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

function Invoke-ElevatedDeviceRescan {
    try {
        $scriptBlock = @'
try { pnputil /scan-devices | Out-Null } catch {}
try { "rescan" | diskpart | Out-Null } catch {}
Start-Sleep -Seconds 2
'@
        $proc = Start-Process -FilePath "powershell.exe" -ArgumentList "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", $scriptBlock -Verb RunAs -PassThru -Wait -WindowStyle Hidden
        return ($proc.ExitCode -eq 0)
    } catch {
        return $false
    }
}

# 1) Early exit if drive is already online
Write-Host "Checking current drive status..." -ForegroundColor Cyan
$disk = Get-Disk | Where-Object {
    $_.SerialNumber -match $serial -or $_.FriendlyName -match $model
} -ErrorAction SilentlyContinue

if ($disk -and -not $disk.IsOffline) {
    Write-Host ""
    Write-Host "DRIVE ALREADY ONLINE" -ForegroundColor Green
    Write-Host "Drive: $($disk.FriendlyName)"
    Write-Host "Disk Number: $($disk.Number)"
    Write-Host ""
    Write-Host "To sleep the drive, run: .\sleep-hdd.ps1" -ForegroundColor Cyan
    exit 0
}

# 2) Turn the relays on
Write-Host "Powering up HDD..." -ForegroundColor Cyan
try {
    $result = Start-Process -FilePath "relay.exe" -ArgumentList "all", "on" -Wait -PassThru -NoNewWindow
    if ($result.ExitCode -ne 0) {
        Write-Error "Failed to activate relay power"
        exit 1
    }
} catch {
    Write-Error "Power activation failed: $($_.Exception.Message)"
    exit 1
}

Start-Sleep -Seconds 2

# 3) Scan for new devices (elevated preferred, fallback non-elevated)
Write-Host "Scanning for new devices..." -ForegroundColor Cyan
$elevated = Invoke-ElevatedDeviceRescan
if (-not $elevated) {
    try { pnputil /scan-devices | Out-Null } catch {}
    try { "rescan" | diskpart | Out-Null } catch {}
}
Start-Sleep -Seconds 2

# Refresh disk info
$disk = Get-Disk | Where-Object {
    $_.SerialNumber -match $serial -or $_.FriendlyName -match $model
} -ErrorAction SilentlyContinue

if (-not $disk) {
    Write-Error "Target drive not detected. It may need more time to initialize."
    exit 1
}

# 4) Ensure drive is not offline; bring online if needed
if ($disk.IsOffline) {
    if ($isAdmin) {
        Write-Host "Bringing disk online..." -ForegroundColor Cyan
        try {
            Set-Disk -Number $disk.Number -IsOffline $false
            Start-Sleep -Seconds 1
            $verify = Get-Disk -Number $disk.Number
            if ($verify.IsOffline) {
                Write-Warning "Disk may not be fully online yet"
            } else {
                Write-Host "Disk brought online" -ForegroundColor Green
            }
        } catch {
            Write-Error "Failed to bring disk online: $($_.Exception.Message)"
            exit 1
        }
    } else {
        Write-Warning "Disk is offline. Run PowerShell as Administrator to bring it online."
        Write-Host "Disk Number: $($disk.Number)"
        exit 1
    }
} else {
    Write-Host "Disk is already online" -ForegroundColor Green
}

Write-Host ""
Write-Host "HDD WAKE COMPLETE" -ForegroundColor Green
Write-Host "Drive: $($disk.FriendlyName)"
Write-Host "Status: Online and ready for use"
Write-Host "Disk Number: $($disk.Number)"
Write-Host ""
Write-Host "To sleep the drive again, run: .\sleep-hdd.ps1" -ForegroundColor Cyan