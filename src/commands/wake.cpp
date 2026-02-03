// Wake HDD Command for HDD Toggle
// Powers on the drive via relay and initializes it in Windows
// Converted from wake-hdd.c to C++ for unified binary

#include "commands.h"
#include "hdd-toggle.h"
#include "core/process.h"
#include "core/admin.h"
#include "core/disk.h"
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace hdd {
namespace commands {

namespace {

const int MAX_COMMAND_LEN = 1024;

// Check if disk is already online and available
bool IsDiskOnline() {
    char command[MAX_COMMAND_LEN];
    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-Disk | Where-Object { $_.SerialNumber -match '%s' -or $_.FriendlyName -match '%s' } -ErrorAction SilentlyContinue; "
        "if ($disk -and -not $disk.IsOffline) { exit 0 } else { exit 1 }\"",
        DEFAULT_TARGET_SERIAL, DEFAULT_TARGET_MODEL);

    return core::ExecuteCommand(command, true) == 0;
}

// Get disk information for status display
bool GetDiskInfo(std::string& friendlyName, int& diskNumber) {
    char command[MAX_COMMAND_LEN];
    const char* tempFile = "disk_info.tmp";

    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-Disk | Where-Object { $_.SerialNumber -match '%s' -or $_.FriendlyName -match '%s' } -ErrorAction SilentlyContinue; "
        "if ($disk) { Write-Output \\\"$($disk.FriendlyName)|$($disk.Number)\\\" | Out-File -FilePath '%s' -Encoding ASCII }\"",
        DEFAULT_TARGET_SERIAL, DEFAULT_TARGET_MODEL, tempFile);

    if (core::ExecuteCommand(command, true) == 0) {
        Sleep(500); // Give file time to be written
        FILE* fp = fopen(tempFile, "r");
        if (fp) {
            char line[256];
            if (fgets(line, sizeof(line), fp)) {
                // Remove trailing newline/whitespace
                line[strcspn(line, "\r\n")] = '\0';

                char* pipePos = strchr(line, '|');
                if (pipePos) {
                    *pipePos = '\0';
                    friendlyName = line;
                    diskNumber = atoi(pipePos + 1);
                    fclose(fp);
                    DeleteFileA(tempFile);
                    return true;
                }
            }
            fclose(fp);
        }
    }

    DeleteFileA(tempFile);
    return false;
}

// Try to perform elevated device rescan
bool TryElevatedDeviceRescan() {
    printf("Attempting elevated device rescan...\n");

    HINSTANCE result = ShellExecuteA(NULL, "runas", "powershell.exe",
        "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command \"try { pnputil /scan-devices | Out-Null } catch {} try { 'rescan' | diskpart | Out-Null } catch {} Start-Sleep -Seconds 2\"",
        NULL, SW_HIDE);

    // ShellExecute returns > 32 on success
    if ((INT_PTR)result > 32) {
        Sleep(6000); // Give time for elevated process to complete
        return true;
    }

    printf("Elevated rescan failed or cancelled.\n");
    return false;
}

// Perform non-elevated device rescan
void PerformBasicDeviceRescan() {
    printf("Performing basic device rescan...\n");
    core::ExecuteCommand("pnputil /scan-devices", true);
    core::ExecuteCommand("echo rescan | diskpart", true);
}

// Check if disk is offline and try to bring it online
bool BringDiskOnline() {
    char command[MAX_COMMAND_LEN];

    // First check if disk is offline
    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-Disk | Where-Object { $_.SerialNumber -match '%s' -or $_.FriendlyName -match '%s' } -ErrorAction SilentlyContinue; "
        "if ($disk -and $disk.IsOffline) { exit 1 } else { exit 0 }\"",
        DEFAULT_TARGET_SERIAL, DEFAULT_TARGET_MODEL);

    if (core::ExecuteCommand(command, true) == 0) {
        printf("Disk is already online\n");
        return true;
    }

    // Disk is offline, try to bring it online
    if (!core::IsRunningAsAdmin()) {
        printf("WARNING: Disk is offline but not running as Administrator.\n");
        printf("Please run as Administrator to bring disk online.\n");
        return false;
    }

    printf("Bringing disk online...\n");
    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-Disk | Where-Object { $_.SerialNumber -match '%s' -or $_.FriendlyName -match '%s' } -ErrorAction SilentlyContinue; "
        "if ($disk -and $disk.IsOffline) { Set-Disk -Number $disk.Number -IsOffline $false; Start-Sleep -Seconds 1 }\"",
        DEFAULT_TARGET_SERIAL, DEFAULT_TARGET_MODEL);

    if (core::ExecuteCommand(command, true) == 0) {
        printf("Disk brought online successfully\n");
        return true;
    } else {
        printf("Failed to bring disk online\n");
        return false;
    }
}

void ShowWakeUsage() {
    printf("Wake HDD - Power on and initialize hard drive\n");
    printf("Usage: hdd-toggle wake [-h|--help]\n\n");
    printf("Target: %s (Serial: %s)\n", DEFAULT_TARGET_MODEL, DEFAULT_TARGET_SERIAL);
}

} // anonymous namespace

int RunWake(int argc, char* argv[]) {
    // Check for help flag
    if (argc >= 1 && core::IsHelpFlag(argv[0])) {
        ShowWakeUsage();
        return EXIT_SUCCESS;
    }

    std::string friendlyName;
    int diskNumber = -1;

    printf("HDD Wake Utility\n");
    printf("Target: %s (Serial: %s)\n\n", DEFAULT_TARGET_MODEL, DEFAULT_TARGET_SERIAL);

    // 1. Check if drive is already online
    printf("Checking current drive status...\n");
    if (IsDiskOnline()) {
        if (GetDiskInfo(friendlyName, diskNumber)) {
            printf("\nDRIVE ALREADY ONLINE\n");
            printf("Drive: %s\n", friendlyName.c_str());
            printf("Disk Number: %d\n\n", diskNumber);
        } else {
            printf("\nDRIVE ALREADY ONLINE\n\n");
        }
        printf("To sleep the drive, run: hdd-toggle sleep\n");
        return EXIT_SUCCESS;
    }

    // 2. Power up relays
    printf("Powering up HDD...\n");
    if (!ControlRelayPower(true)) {
        printf("ERROR: Failed to activate relay power\n");
        return EXIT_OPERATION_FAILED;
    }
    printf("Power ON: Both relays activated\n");

    Sleep(3000); // Wait for drive to initialize

    // 3. Device rescan (try elevated first, fallback to basic)
    printf("Scanning for new devices...\n");
    if (!TryElevatedDeviceRescan()) {
        PerformBasicDeviceRescan();
    }

    Sleep(3000); // Wait for device detection

    // 4. Verify drive is detected (with retry logic)
    printf("Checking for target drive...\n");
    int retryCount = 0;
    int maxRetries = 4; // Try for up to ~12 more seconds

    while (retryCount < maxRetries && !GetDiskInfo(friendlyName, diskNumber)) {
        retryCount++;
        if (retryCount == 1) {
            printf("Drive not detected yet, waiting for initialization...\n");
        }
        Sleep(3000); // Wait 3 seconds between retries
    }

    if (!GetDiskInfo(friendlyName, diskNumber)) {
        printf("ERROR: Target drive not detected after %d seconds.\n", 6 + (maxRetries * 3));
        printf("The drive may need more time to initialize or there may be a hardware issue.\n");
        return EXIT_DEVICE_NOT_FOUND;
    }

    printf("Found drive: %s (Disk %d)\n", friendlyName.c_str(), diskNumber);

    // 5. Ensure drive is online
    if (!BringDiskOnline()) {
        return EXIT_OPERATION_FAILED;
    }

    // 6. Final status
    printf("\nHDD WAKE COMPLETE\n");
    printf("Drive: %s\n", friendlyName.c_str());
    printf("Status: Online and ready for use\n");
    printf("Disk Number: %d\n\n", diskNumber);
    printf("To sleep the drive again, run: hdd-toggle sleep\n");

    return EXIT_SUCCESS;
}

} // namespace commands
} // namespace hdd
