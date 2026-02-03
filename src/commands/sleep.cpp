// Sleep HDD Command for HDD Toggle
// Safely ejects and powers off the drive via relay
// Converted from sleep-hdd.c to C++ for unified binary

#include "commands.h"
#include "hdd-toggle.h"
#include "core/process.h"
#include "core/admin.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace hdd {
namespace commands {

namespace {

const int MAX_COMMAND_LEN = 1024;
const int MAX_PATH_LEN = 512;

struct SleepOptions {
    bool help = false;
    bool offline = false;
};

// Parse command line arguments
SleepOptions ParseSleepArgs(int argc, char* argv[]) {
    SleepOptions opts;

    for (int i = 0; i < argc; i++) {
        if (_stricmp(argv[i], "-help") == 0 || _stricmp(argv[i], "-h") == 0 ||
            _stricmp(argv[i], "/?") == 0 || _stricmp(argv[i], "--help") == 0) {
            opts.help = true;
        }
        else if (_stricmp(argv[i], "-offline") == 0 || _stricmp(argv[i], "--offline") == 0) {
            opts.offline = true;
        }
    }

    return opts;
}

void ShowSleepUsage() {
    printf("Sleep HDD - Safely eject and power down hard drive\n\n");
    printf("Usage: hdd-toggle sleep [--offline] [-h|--help]\n\n");
    printf("Options:\n");
    printf("  --offline    Take disk offline before power down (requires Administrator)\n");
    printf("  -h, --help   Show this help message\n\n");
    printf("Target: %s (Serial: %s)\n\n", DEFAULT_TARGET_MODEL, DEFAULT_TARGET_SERIAL);
    printf("Notes:\n");
    printf("  - Attempts safe removal using various methods\n");
    printf("  - Falls back to relay power-off regardless\n");
}

// Check if target disk exists and get its info
bool GetTargetDiskInfo(std::string& modelOut, int& diskIndex) {
    char command[MAX_COMMAND_LEN];
    const char* tempFile = "disk_sleep_info.tmp";

    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-CimInstance -ClassName Win32_DiskDrive | Where-Object { $_.SerialNumber -match '%s' -or $_.Model -match '%s' } -ErrorAction SilentlyContinue | Select-Object -First 1; "
        "if ($disk) { ($disk.Model + '|' + $disk.Index) | Out-File -FilePath '%s' -Encoding ASCII }\"",
        DEFAULT_TARGET_SERIAL, DEFAULT_TARGET_MODEL, tempFile);

    if (core::ExecuteCommand(command, true) == 0) {
        Sleep(500);
        FILE* fp = fopen(tempFile, "r");
        if (fp) {
            char line[256];
            if (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\r\n")] = '\0';

                char* pipePos = strchr(line, '|');
                if (pipePos) {
                    *pipePos = '\0';
                    modelOut = line;
                    diskIndex = atoi(pipePos + 1);
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

// Get drive letters for the target disk
std::vector<std::string> GetDriveLetters() {
    std::vector<std::string> letters;
    char command[MAX_COMMAND_LEN];
    const char* tempFile = "drive_letters.tmp";

    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-CimInstance Win32_DiskDrive | Where-Object { $_.SerialNumber -match '%s' -or $_.Model -match '%s' } | Select-Object -First 1; "
        "if ($disk) { "
        "$parts = Get-CimAssociatedInstance -InputObject $disk -Association Win32_DiskDriveToDiskPartition -ErrorAction SilentlyContinue; "
        "foreach ($p in $parts) { "
        "$ldisks = Get-CimAssociatedInstance -InputObject $p -Association Win32_LogicalDiskToPartition -ErrorAction SilentlyContinue; "
        "foreach ($ld in $ldisks) { if ($ld.DeviceID) { $ld.DeviceID | Out-File -FilePath '%s' -Encoding ASCII -Append } } } }\"",
        DEFAULT_TARGET_SERIAL, DEFAULT_TARGET_MODEL, tempFile);

    // Delete temp file first since we're using -Append
    DeleteFileA(tempFile);

    if (core::ExecuteCommand(command, true) == 0) {
        Sleep(500);
        FILE* fp = fopen(tempFile, "r");
        if (fp) {
            char line[16];
            while (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\r\n")] = '\0';
                if (strlen(line) > 0 && strlen(line) < 4) {
                    letters.push_back(std::string(line));
                }
            }
            fclose(fp);
        }
    }

    DeleteFileA(tempFile);
    return letters;
}

// Find RemoveDrive.exe in PATH or current directory
std::string FindRemoveDrive() {
    return core::FindExecutable("RemoveDrive.exe");
}

// Attempt safe removal using RemoveDrive.exe with proper retry logic
bool AttemptSafeRemoval(const std::vector<std::string>& letters) {
    std::string removeDrivePath = FindRemoveDrive();

    if (removeDrivePath.empty()) {
        printf("RemoveDrive.exe not found on PATH or current directory. Skipping safe removal and powering off.\n");
        return false;
    }

    printf("Found RemoveDrive.exe: %s\n", removeDrivePath.c_str());

    // Try each drive letter with retries
    for (int retry = 1; retry <= 3; retry++) {
        for (const auto& letter : letters) {
            char command[MAX_COMMAND_LEN];
            snprintf(command, sizeof(command), "\"%s\" %s -b", removeDrivePath.c_str(), letter.c_str());

            printf("RemoveDrive attempt %d: %s -b\n", retry, letter.c_str());

            int exitCode = core::ExecuteCommand(command, false);
            if (exitCode == 0) {
                printf("Safe removal succeeded via RemoveDrive (%s)\n", letter.c_str());
                return true;
            } else {
                printf("RemoveDrive failed for %s (exit code: %d)\n", letter.c_str(), exitCode);
            }
        }

        if (retry < 3) {
            printf("Retrying in 2 seconds...\n");
            Sleep(2000);
        }
    }

    printf("Safe removal did not complete after retries; proceeding anyway.\n");
    return false;
}

// Take disk offline using diskpart (requires admin)
bool TakeDiskOffline(int diskIndex) {
    if (!core::IsRunningAsAdmin()) {
        printf("WARNING: --offline requested but not running as Administrator. Skipping offline.\n");
        return false;
    }

    printf("Taking disk offline via diskpart (Disk %d)...\n", diskIndex);

    char command[MAX_COMMAND_LEN];
    snprintf(command, sizeof(command),
        "echo select disk %d & echo offline disk | diskpart",
        diskIndex);

    if (core::ExecuteCommand(command, true) == 0) {
        printf("Disk taken offline successfully\n");
        Sleep(1000);
        return true;
    } else {
        printf("diskpart offline failed\n");
        return false;
    }
}

// Control relay via the relay command
bool ControlRelayPower(bool on) {
    char* args[] = { const_cast<char*>(on ? "on" : "off") };
    return RunRelay(1, args) == EXIT_SUCCESS;
}

} // anonymous namespace

int RunSleep(int argc, char* argv[]) {
    SleepOptions opts = ParseSleepArgs(argc, argv);

    printf("HDD Sleep Utility\n");
    printf("Target: %s (Serial: %s)\n\n", DEFAULT_TARGET_MODEL, DEFAULT_TARGET_SERIAL);

    // Show help if requested
    if (opts.help) {
        ShowSleepUsage();
        return EXIT_SUCCESS;
    }

    // 1. Locate target disk
    printf("Locating target disk...\n");
    std::string model;
    int diskIndex = -1;

    bool diskFound = GetTargetDiskInfo(model, diskIndex);

    if (!diskFound) {
        printf("Target disk not found. Proceeding to power down relays anyway.\n");
    } else {
        printf("Found disk: %s (Index: %d)\n", model.c_str(), diskIndex);

        // 2. Get drive letters for safe removal
        std::vector<std::string> letters = GetDriveLetters();

        if (!letters.empty()) {
            printf("Found %zu drive letter(s): ", letters.size());
            for (const auto& letter : letters) {
                printf("%s ", letter.c_str());
            }
            printf("\n");

            // 3. Attempt safe removal
            bool safeRemovalSucceeded = AttemptSafeRemoval(letters);
            if (!safeRemovalSucceeded) {
                printf("WARNING: Safe removal failed - drive may not have been safely ejected\n");
            }
        } else {
            printf("No drive letters found for target disk.\n");
        }

        // 4. Optional: Take disk offline
        if (opts.offline) {
            TakeDiskOffline(diskIndex);
        }
    }

    // 5. Always power down relays
    printf("Powering down HDD...\n");
    if (!ControlRelayPower(false)) {
        printf("ERROR: Failed to deactivate relay power\n");
        return EXIT_OPERATION_FAILED;
    }
    printf("Power OFF: Both relays deactivated\n");

    // 6. Final status
    printf("\n");
    if (diskFound) {
        printf("HDD SLEEP COMPLETE\n");
        printf("Drive: %s\n", model.c_str());
    } else {
        printf("HDD POWER DOWN COMPLETE\n");
        printf("Drive not detected by Windows at time of power down\n");
    }
    printf("\n");
    printf("To wake the drive again, run: hdd-toggle wake\n");

    return EXIT_SUCCESS;
}

} // namespace commands
} // namespace hdd
