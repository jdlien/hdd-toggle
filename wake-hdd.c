// Wake HDD - C Implementation
// Equivalent to wake-hdd.ps1 but as a standalone executable
// Target: WDC WD181KFGX-68AFPN0 (SN: 2VH7TM9L)

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "shell32.lib")

#define TARGET_SERIAL "2VH7TM9L"
#define TARGET_MODEL "WDC WD181KFGX-68AFPN0"
#define MAX_COMMAND_LEN 1024

// ANSI color codes for console output
#define COLOR_RESET   ""
#define COLOR_RED     ""
#define COLOR_GREEN   ""
#define COLOR_YELLOW  ""
#define COLOR_CYAN    ""

// Check if running as administrator
BOOL IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    HANDLE token = NULL;
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);
        
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
            isAdmin = elevation.TokenIsElevated;
        }
        CloseHandle(token);
    }
    
    return isAdmin;
}

// Execute command and return exit code
int ExecuteCommand(const char* command, BOOL hide_window) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 1;
    
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = hide_window ? SW_HIDE : SW_NORMAL;
    ZeroMemory(&pi, sizeof(pi));
    
    // Create a modifiable copy of the command string
    char* cmd_copy = _strdup(command);
    if (!cmd_copy) return 1;
    
    if (CreateProcess(NULL, cmd_copy, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    free(cmd_copy);
    return (int)exit_code;
}

// Check if disk is already online and available
BOOL IsDiskOnline() {
    char command[MAX_COMMAND_LEN];
    snprintf(command, sizeof(command), 
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-Disk | Where-Object { $_.SerialNumber -match '%s' -or $_.FriendlyName -match '%s' } -ErrorAction SilentlyContinue; "
        "if ($disk -and -not $disk.IsOffline) { exit 0 } else { exit 1 }\"",
        TARGET_SERIAL, TARGET_MODEL);
    
    return ExecuteCommand(command, TRUE) == 0;
}

// Get disk information for status display
BOOL GetDiskInfo(char* friendly_name, int* disk_number) {
    char command[MAX_COMMAND_LEN];
    char temp_file[] = "disk_info.tmp";
    FILE* fp;
    
    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-Disk | Where-Object { $_.SerialNumber -match '%s' -or $_.FriendlyName -match '%s' } -ErrorAction SilentlyContinue; "
        "if ($disk) { Write-Output \\\"$($disk.FriendlyName)|$($disk.Number)\\\" }\" > %s",
        TARGET_SERIAL, TARGET_MODEL, temp_file);
    
    if (ExecuteCommand(command, TRUE) == 0) {
        fp = fopen(temp_file, "r");
        if (fp) {
            char line[256];
            if (fgets(line, sizeof(line), fp)) {
                char* pipe_pos = strchr(line, '|');
                if (pipe_pos) {
                    *pipe_pos = '\0';
                    strcpy(friendly_name, line);
                    *disk_number = atoi(pipe_pos + 1);
                    fclose(fp);
                    DeleteFile(temp_file);
                    return TRUE;
                }
            }
            fclose(fp);
        }
    }
    
    DeleteFile(temp_file);
    return FALSE;
}

// Try to perform elevated device rescan
BOOL TryElevatedDeviceRescan() {
    printf("Attempting elevated device rescan...\n");
    
    HINSTANCE result = ShellExecute(NULL, L"runas", L"powershell.exe", 
        L"-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command \"try { pnputil /scan-devices | Out-Null } catch {} try { 'rescan' | diskpart | Out-Null } catch {} Start-Sleep -Seconds 2\"",
        NULL, SW_HIDE);
    
    // ShellExecute returns > 32 on success
    if ((INT_PTR)result > 32) {
        Sleep(4000); // Give time for elevated process to complete
        return TRUE;
    }
    
    printf("Elevated rescan failed or cancelled.\n");
    return FALSE;
}

// Perform non-elevated device rescan
void PerformBasicDeviceRescan() {
    printf("Performing basic device rescan...\n");
    ExecuteCommand("pnputil /scan-devices", TRUE);
    ExecuteCommand("echo rescan | diskpart", TRUE);
}

// Check if disk is offline and try to bring it online
BOOL BringDiskOnline() {
    char command[MAX_COMMAND_LEN];
    
    // First check if disk is offline
    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-Disk | Where-Object { $_.SerialNumber -match '%s' -or $_.FriendlyName -match '%s' } -ErrorAction SilentlyContinue; "
        "if ($disk -and $disk.IsOffline) { exit 1 } else { exit 0 }\"",
        TARGET_SERIAL, TARGET_MODEL);
    
    if (ExecuteCommand(command, TRUE) == 0) {
        printf("Disk is already online\n");
        return TRUE;
    }
    
    // Disk is offline, try to bring it online
    if (!IsRunningAsAdmin()) {
        printf("WARNING: Disk is offline but not running as Administrator.\n");
        printf("Please run as Administrator to bring disk online.\n");
        return FALSE;
    }
    
    printf("Bringing disk online...\n");
    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-Disk | Where-Object { $_.SerialNumber -match '%s' -or $_.FriendlyName -match '%s' } -ErrorAction SilentlyContinue; "
        "if ($disk -and $disk.IsOffline) { Set-Disk -Number $disk.Number -IsOffline $false; Start-Sleep -Seconds 1 }\"",
        TARGET_SERIAL, TARGET_MODEL);
    
    if (ExecuteCommand(command, TRUE) == 0) {
        printf("Disk brought online successfully\n");
        return TRUE;
    } else {
        printf("Failed to bring disk online\n");
        return FALSE;
    }
}

int main(int argc, char* argv[]) {
    char friendly_name[256] = {0};
    int disk_number = -1;
    
    printf("HDD Wake Utility\n");
    printf("Target: %s (Serial: %s)\n\n", TARGET_MODEL, TARGET_SERIAL);
    
    // 1. Check if drive is already online
    printf("Checking current drive status...\n");
    if (IsDiskOnline()) {
        if (GetDiskInfo(friendly_name, &disk_number)) {
            printf("\nDRIVE ALREADY ONLINE\n");
            printf("Drive: %s\n", friendly_name);
            printf("Disk Number: %d\n\n", disk_number);
        } else {
            printf("\nDRIVE ALREADY ONLINE\n\n");
        }
        printf("To sleep the drive, run: sleep-hdd.exe\n");
        return 0;
    }
    
    // 2. Power up relays
    printf("Powering up HDD...\n");
    if (ExecuteCommand("relay.exe all on", FALSE) != 0) {
        printf("ERROR: Failed to activate relay power\n");
        return 1;
    }
    printf("Power ON: Both relays activated\n");
    
    Sleep(2000); // Wait for drive to initialize
    
    // 3. Device rescan (try elevated first, fallback to basic)
    printf("Scanning for new devices...\n");
    if (!TryElevatedDeviceRescan()) {
        PerformBasicDeviceRescan();
    }
    
    Sleep(2000); // Wait for device detection
    
    // 4. Verify drive is detected
    printf("Checking for target drive...\n");
    if (!GetDiskInfo(friendly_name, &disk_number)) {
        printf("ERROR: Target drive not detected after device rescan.\n");
        printf("The drive may need more time to initialize.\n");
        return 1;
    }
    
    printf("Found drive: %s (Disk %d)\n", friendly_name, disk_number);
    
    // 5. Ensure drive is online
    if (!BringDiskOnline()) {
        return 1;
    }
    
    // 6. Final status
    printf("\nHDD WAKE COMPLETE\n");
    printf("Drive: %s\n", friendly_name);
    printf("Status: Online and ready for use\n");
    printf("Disk Number: %d\n\n", disk_number);
    printf("To sleep the drive again, run: sleep-hdd.exe\n");
    
    return 0;
}