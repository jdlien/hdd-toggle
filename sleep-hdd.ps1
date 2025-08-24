# Sleep HDD Script - RemoveDrive-based
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
    Write-Host "Notes:"
    Write-Host "  - Uses RemoveDrive.exe (on PATH) to request safe removal (with -b)"
    Write-Host "  - Falls back to relay power-off regardless"
    exit 0
}

$serial = '2VH7TM9L'
$model  = 'WDC WD181KFGX-68AFPN0'

function Resolve-RemoveDrivePath {
    try {
        $cmd = Get-Command RemoveDrive.exe -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
        $local = Join-Path -Path (Get-Location) -ChildPath 'RemoveDrive.exe'
        if (Test-Path $local) { return $local }
        return $null
    } catch { return $null }
}

function Get-TargetDiskCim {
    try {
        $disks = Get-CimInstance -ClassName Win32_DiskDrive -ErrorAction Stop
        return $disks | Where-Object { $_.SerialNumber -match $serial -or $_.Model -match $model } | Select-Object -First 1
    } catch { return $null }
}

function Get-DiskLettersAndVolumes {
    param(
        [Parameter(Mandatory=$true)] $diskCim
    )
    $resultLetters = @()
    $resultVolumes = @()
    try {
        $parts = Get-CimAssociatedInstance -InputObject $diskCim -Association Win32_DiskDriveToDiskPartition -ErrorAction Stop
        foreach ($p in $parts) {
            $ldisks = Get-CimAssociatedInstance -InputObject $p -Association Win32_LogicalDiskToPartition -ErrorAction SilentlyContinue
            foreach ($ld in $ldisks) {
                if ($ld.DeviceID) { $resultLetters += $ld.DeviceID.TrimEnd(':') }
            }
        }
    } catch {}

    # Map letters to volume GUIDs via Win32_Volume (no Storage module required)
    if ($resultLetters.Count -gt 0) {
        try {
            $vols = Get-CimInstance -ClassName Win32_Volume -ErrorAction Stop
            foreach ($v in $vols) {
                if ($v.DriveLetter) {
                    $dl = $v.DriveLetter.TrimEnd(':')
                    if ($resultLetters -contains $dl) {
                        if ($v.DeviceID) { $resultVolumes += $v.DeviceID }
                    }
                }
            }
        } catch {}
    }

    return [PSCustomObject]@{
        Letters = ($resultLetters | Select-Object -Unique)
        Volumes = ($resultVolumes | Select-Object -Unique)
    }
}

function Invoke-RemoveDriveWithRetries {
    param(
        [Parameter(Mandatory=$true)][string[]] $Targets,
        [int] $MaxTries = 3,
        [int] $DelaySeconds = 2
    )
    $rd = Resolve-RemoveDrivePath
    if (-not $rd) {
        Write-Host "RemoveDrive.exe not found on PATH. Skipping safe removal and powering off." -ForegroundColor Yellow
        return $false
    }

    for ($i = 1; $i -le $MaxTries; $i++) {
        foreach ($t in $Targets) {
            Write-Host "RemoveDrive attempt $($i): $($t) -b" -ForegroundColor Cyan
            try {
                $proc = Start-Process -FilePath $rd -ArgumentList $t, '-b' -Wait -PassThru -NoNewWindow
                if ($proc.ExitCode -eq 0) {
                    Write-Host "Safe removal succeeded via RemoveDrive ($t)" -ForegroundColor Green
                    return $true
                }
            } catch {
                Write-Host "RemoveDrive failed for $($t): $($_.Exception.Message)" -ForegroundColor Yellow
            }
        }
        Start-Sleep -Seconds $DelaySeconds
    }
    return $false
}

# Discover target disk and identifiers
Write-Host "Locating target disk..." -ForegroundColor Cyan
$diskCim = Get-TargetDiskCim
$letters = @()
$volumes = @()
$diskIndex = $null
if ($diskCim) {
    $diskIndex = $diskCim.Index
    $ids = Get-DiskLettersAndVolumes -diskCim $diskCim
    $letters = $ids.Letters | ForEach-Object { "{0}" -f $_ }
    $volumes = $ids.Volumes
}

# Build target list for RemoveDrive: prefer letters first, then volume GUIDs
$targets = @()
foreach ($L in $letters) { $targets += ("$($L):") }
foreach ($vg in $volumes) { $targets += $vg }

if ($targets.Count -eq 0) {
    Write-Host "No drive letters or volumes found for target; proceeding to power down relays." -ForegroundColor Yellow
} else {
    $removed = Invoke-RemoveDriveWithRetries -Targets $targets -MaxTries 3 -DelaySeconds 2
    if (-not $removed) {
        Write-Host "Safe removal did not complete after retries; proceeding anyway." -ForegroundColor Yellow
    }
}

# Optional: take disk offline (admin) using diskpart, only if disk index known
if ($Offline -and $null -ne $diskIndex) {
    $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")
    if ($isAdmin) {
        Write-Host "Taking disk offline via diskpart (Disk $diskIndex)..." -ForegroundColor Cyan
        try {
            $commands = @(
                "select disk $diskIndex",
                "offline disk"
            )
            $commands | diskpart | Out-Null
            Start-Sleep -Seconds 1
        } catch {
            Write-Host "diskpart offline failed: $($_.Exception.Message)" -ForegroundColor Yellow
        }
    } else {
        Write-Host "-Offline requested, but script is not elevated; skipping offline." -ForegroundColor Yellow
    }
}

# Always power down relays regardless of detection state
Write-Host "Powering down HDD..." -ForegroundColor Cyan
try {
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

Write-Host ""
if ($diskCim) {
    Write-Host "HDD SLEEP COMPLETE" -ForegroundColor Green
    Write-Host "Drive: $($diskCim.Model)"
} else {
    Write-Host "HDD POWER DOWN COMPLETE" -ForegroundColor Yellow
    Write-Host "Drive not detected by Windows at time of power down"
}
Write-Host ""
Write-Host "To wake the drive again, run: .\wake-hdd.ps1" -ForegroundColor Cyan