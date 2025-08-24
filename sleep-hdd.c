// Sleep HDD - C Implementation
// Equivalent to sleep-hdd.ps1 but as a standalone executable
// Target: WDC WD181KFGX-68AFPN0 (SN: 2VH7TM9L)

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

#define TARGET_SERIAL "2VH7TM9L"
#define TARGET_MODEL "WDC WD181KFGX-68AFPN0"
#define MAX_COMMAND_LEN 1024
#define MAX_PATH_LEN 512

// Command line options
typedef struct {
    BOOL help;
    BOOL offline;
} Options;

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

// Parse command line arguments
Options ParseArguments(int argc, char* argv[]) {
    Options opts = {FALSE, FALSE};
    
    for (int i = 1; i < argc; i++) {
        if (_stricmp(argv[i], "-help") == 0 || _stricmp(argv[i], "-h") == 0 || 
            _stricmp(argv[i], "/?") == 0 || _stricmp(argv[i], "--help") == 0) {
            opts.help = TRUE;
        }
        else if (_stricmp(argv[i], "-offline") == 0) {
            opts.offline = TRUE;
        }
    }
    
    return opts;
}

// Show help message
void ShowHelp() {
    printf("Sleep HDD Script - Power down hard drive safely\n\n");
    printf("Usage: sleep-hdd.exe [-Offline] [-Help]\n\n");
    printf("Parameters:\n");
    printf("  -Offline    Take disk offline before power down (requires Administrator)\n");
    printf("  -Help, -h   Show this help message\n\n");
    printf("Notes:\n");
    printf("  - Attempts safe removal using various methods\n");
    printf("  - Falls back to relay power-off regardless\n");
}

// Check if target disk exists and get its info
BOOL GetTargetDiskInfo(char* model_out, int* disk_index) {
    char command[MAX_COMMAND_LEN];
    char temp_file[] = "disk_sleep_info.tmp";
    FILE* fp;
    
    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-CimInstance -ClassName Win32_DiskDrive | Where-Object { $_.SerialNumber -match '%s' -or $_.Model -match '%s' } -ErrorAction SilentlyContinue | Select-Object -First 1; "
        "if ($disk) { '$($disk.Model)|$($disk.Index)' | Out-File -FilePath '%s' -Encoding ASCII }\"",
        TARGET_SERIAL, TARGET_MODEL, temp_file);
    
    if (ExecuteCommand(command, TRUE) == 0) {
        Sleep(500);
        fp = fopen(temp_file, "r");
        if (fp) {
            char line[256];
            if (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\r\n")] = '\0';
                
                char* pipe_pos = strchr(line, '|');
                if (pipe_pos) {
                    *pipe_pos = '\0';
                    strcpy(model_out, line);
                    *disk_index = atoi(pipe_pos + 1);
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

// Get drive letters for the target disk
int GetDriveLetters(char letters[][4], int max_letters) {
    char command[MAX_COMMAND_LEN];
    char temp_file[] = "drive_letters.tmp";
    FILE* fp;
    int count = 0;
    
    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-CimInstance Win32_DiskDrive | Where-Object { $_.SerialNumber -match '%s' -or $_.Model -match '%s' } | Select-Object -First 1; "
        "if ($disk) { "
        "$parts = Get-CimAssociatedInstance -InputObject $disk -Association Win32_DiskDriveToDiskPartition -ErrorAction SilentlyContinue; "
        "foreach ($p in $parts) { "
        "$ldisks = Get-CimAssociatedInstance -InputObject $p -Association Win32_LogicalDiskToPartition -ErrorAction SilentlyContinue; "
        "foreach ($ld in $ldisks) { if ($ld.DeviceID) { $ld.DeviceID | Out-File -FilePath '%s' -Encoding ASCII -Append } } } }\"",
        TARGET_SERIAL, TARGET_MODEL, temp_file);
    
    // Delete temp file first since we're using -Append
    DeleteFile(temp_file);
    
    if (ExecuteCommand(command, TRUE) == 0) {
        Sleep(500);
        fp = fopen(temp_file, "r");
        if (fp) {
            char line[16];
            while (fgets(line, sizeof(line), fp) && count < max_letters) {
                line[strcspn(line, "\r\n")] = '\0';
                if (strlen(line) > 0 && strlen(line) < 4) {
                    strcpy(letters[count], line);
                    count++;
                }
            }
            fclose(fp);
        }
    }
    
    DeleteFile(temp_file);
    return count;
}

// Find RemoveDrive.exe in PATH or current directory
BOOL FindRemoveDrive(char* path_out) {
    // Try current directory first
    if (GetFileAttributes("RemoveDrive.exe") != INVALID_FILE_ATTRIBUTES) {
        strcpy(path_out, "RemoveDrive.exe");
        return TRUE;
    }
    
    // Search in PATH
    char* path_env = getenv("PATH");
    if (!path_env) return FALSE;
    
    char* path_copy = _strdup(path_env);
    if (!path_copy) return FALSE;
    
    char* token = strtok(path_copy, ";");
    while (token) {
        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s\\RemoveDrive.exe", token);
        
        if (GetFileAttributes(full_path) != INVALID_FILE_ATTRIBUTES) {
            strcpy(path_out, full_path);
            free(path_copy);
            return TRUE;
        }
        
        token = strtok(NULL, ";");
    }
    
    free(path_copy);
    return FALSE;
}

// Attempt safe removal using RemoveDrive.exe with proper retry logic
BOOL AttemptSafeRemoval(char letters[][4], int letter_count) {
    char removedrive_path[MAX_PATH_LEN];
    
    if (!FindRemoveDrive(removedrive_path)) {
        printf("RemoveDrive.exe not found on PATH or current directory. Skipping safe removal and powering off.\n");
        return FALSE;
    }
    
    printf("Found RemoveDrive.exe: %s\n", removedrive_path);
    
    // Try each drive letter with retries (matches PowerShell logic)
    for (int retry = 1; retry <= 3; retry++) {
        for (int i = 0; i < letter_count; i++) {
            char command[MAX_COMMAND_LEN];
            snprintf(command, sizeof(command), "\"%s\" %s -b", removedrive_path, letters[i]);
            
            printf("RemoveDrive attempt %d: %s -b\n", retry, letters[i]);
            
            int exit_code = ExecuteCommand(command, FALSE);
            if (exit_code == 0) {
                printf("Safe removal succeeded via RemoveDrive (%s)\n", letters[i]);
                return TRUE;
            } else {
                printf("RemoveDrive failed for %s (exit code: %d)\n", letters[i], exit_code);
            }
        }
        
        if (retry < 3) {
            printf("Retrying in 2 seconds...\n");
            Sleep(2000);
        }
    }
    
    printf("Safe removal did not complete after retries; proceeding anyway.\n");
    return FALSE;
}

// Take disk offline using diskpart (requires admin)
BOOL TakeDiskOffline(int disk_index) {
    if (!IsRunningAsAdmin()) {
        printf("WARNING: -Offline requested but not running as Administrator. Skipping offline.\n");
        return FALSE;
    }
    
    printf("Taking disk offline via diskpart (Disk %d)...\n", disk_index);
    
    char command[MAX_COMMAND_LEN];
    snprintf(command, sizeof(command),
        "echo select disk %d & echo offline disk | diskpart",
        disk_index);
    
    if (ExecuteCommand(command, TRUE) == 0) {
        printf("Disk taken offline successfully\n");
        Sleep(1000);
        return TRUE;
    } else {
        printf("diskpart offline failed\n");
        return FALSE;
    }
}

int main(int argc, char* argv[]) {
    Options opts = ParseArguments(argc, argv);
    
    printf("HDD Sleep Utility\n");
    printf("Target: %s (Serial: %s)\n\n", TARGET_MODEL, TARGET_SERIAL);
    
    // Show help if requested
    if (opts.help) {
        ShowHelp();
        return 0;
    }
    
    // 1. Locate target disk
    printf("Locating target disk...\n");
    char model[256] = {0};
    int disk_index = -1;
    
    BOOL disk_found = GetTargetDiskInfo(model, &disk_index);
    
    if (!disk_found) {
        printf("Target disk not found. Proceeding to power down relays anyway.\n");
    } else {
        printf("Found disk: %s (Index: %d)\n", model, disk_index);
        
        // 2. Get drive letters for safe removal
        char letters[8][4];
        int letter_count = GetDriveLetters(letters, 8);
        
        if (letter_count > 0) {
            printf("Found %d drive letter(s): ", letter_count);
            for (int i = 0; i < letter_count; i++) {
                printf("%s ", letters[i]);
            }
            printf("\n");
            
            // 3. Attempt safe removal
            BOOL safe_removal_succeeded = AttemptSafeRemoval(letters, letter_count);
            if (!safe_removal_succeeded) {
                printf("WARNING: Safe removal failed - drive may not have been safely ejected\n");
            }
        } else {
            printf("No drive letters found for target disk.\n");
        }
        
        // 4. Optional: Take disk offline
        if (opts.offline) {
            TakeDiskOffline(disk_index);
        }
    }
    
    // 5. Always power down relays
    printf("Powering down HDD...\n");
    if (ExecuteCommand("relay.exe all off", FALSE) != 0) {
        printf("ERROR: Failed to deactivate relay power\n");
        return 1;
    }
    printf("Power OFF: Both relays deactivated\n");
    
    // 6. Final status
    printf("\n");
    if (disk_found) {
        printf("HDD SLEEP COMPLETE\n");
        printf("Drive: %s\n", model);
    } else {
        printf("HDD POWER DOWN COMPLETE\n");
        printf("Drive not detected by Windows at time of power down\n");
    }
    printf("\n");
    printf("To wake the drive again, run: wake-hdd.exe\n");
    
    return 0;
}