# Trigger Device Detection (No Admin Required)
# Uses scheduled task created by setup-device-task.ps1

$taskName = "HDD-DeviceDetection"

Write-Host "Triggering device detection..." -ForegroundColor Cyan

try {
    # Check if task exists
    $taskExists = schtasks /query /tn $taskName 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Device detection task not found. Run setup-device-task.ps1 as Administrator first."
        exit 1
    }
    
    # Trigger the task
    schtasks /run /tn $taskName | Out-Null
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Device detection task started" -ForegroundColor Green
        Write-Host "Waiting for task to complete..." -ForegroundColor Yellow
        
        # Wait for task to finish (max 10 seconds)
        $timeout = 10
        $elapsed = 0
        
        do {
            Start-Sleep -Seconds 1
            $elapsed++
            
            # Check task status
            $status = schtasks /query /tn $taskName /fo csv | ConvertFrom-Csv | Select-Object -Last 1
            
            if ($status.'Last Run Time' -ne "N/A" -and $status.'Last Task Result' -eq "0") {
                Write-Host "✓ Device detection completed successfully" -ForegroundColor Green
                break
            } elseif ($status.'Last Task Result' -ne "267011") { # 267011 = task running
                if ($status.'Last Task Result' -ne "0") {
                    Write-Warning "Task completed with code: $($status.'Last Task Result')"
                }
                break
            }
            
            if ($elapsed -ge $timeout) {
                Write-Warning "Task is taking longer than expected, continuing anyway..."
                break
            }
            
        } while ($elapsed -lt $timeout)
        
        Write-Host "Device detection trigger complete" -ForegroundColor Green
        return $true
        
    } else {
        Write-Error "Failed to start device detection task"
        return $false
    }
    
} catch {
    Write-Error "Error triggering device detection: $($_.Exception.Message)"
    return $false
}