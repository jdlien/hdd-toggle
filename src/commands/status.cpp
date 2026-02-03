// Status Command for HDD Toggle
// Shows current drive status, with optional JSON output for scripting
// New command for unified binary

#include "commands.h"
#include "hdd-toggle.h"
#include "hdd-utils.h"
#include "core/disk.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace hdd {
namespace commands {

namespace {

struct StatusOptions {
    bool help = false;
    bool json = false;
};

StatusOptions ParseStatusArgs(int argc, char* argv[]) {
    StatusOptions opts;

    for (int i = 0; i < argc; i++) {
        if (core::IsHelpFlag(argv[i])) {
            opts.help = true;
        }
        else if (_stricmp(argv[i], "--json") == 0 || _stricmp(argv[i], "-j") == 0) {
            opts.json = true;
        }
    }

    return opts;
}

void ShowStatusUsage() {
    printf("Drive Status - Show current hard drive status\n\n");
    printf("Usage: hdd-toggle status [--json] [-h|--help]\n\n");
    printf("Options:\n");
    printf("  --json, -j   Output in JSON format for scripting\n");
    printf("  -h, --help   Show this help message\n\n");
    printf("Target: %s (Serial: %s)\n", DEFAULT_TARGET_MODEL, DEFAULT_TARGET_SERIAL);
}

void OutputJson(const core::DriveInfo& info) {
    if (!info.found) {
        printf("{\"status\":\"offline\",\"found\":false}\n");
        return;
    }

    const char* status = (info.state == DriveState::Online) ? "online" : "offline";
    printf("{\"status\":\"%s\",\"found\":true,\"serial\":\"%s\",\"model\":\"%s\",\"disk\":%d}\n",
           status,
           info.serialNumber.c_str(),
           info.model.c_str(),
           info.diskNumber);
}

void OutputText(const core::DriveInfo& info) {
    if (!info.found) {
        printf("Drive: OFFLINE (not detected)\n");
        printf("Target: %s (Serial: %s)\n", DEFAULT_TARGET_MODEL, DEFAULT_TARGET_SERIAL);
        return;
    }

    const char* status = (info.state == DriveState::Online) ? "ONLINE" : "OFFLINE";
    printf("Drive: %s\n", status);
    printf("Model: %s\n", info.model.c_str());
    printf("Serial: %s\n", info.serialNumber.c_str());
    printf("Disk Number: %d\n", info.diskNumber);
}

} // anonymous namespace

int RunStatus(int argc, char* argv[]) {
    StatusOptions opts = ParseStatusArgs(argc, argv);

    if (opts.help) {
        ShowStatusUsage();
        return EXIT_SUCCESS;
    }

    core::DriveInfo info = core::DetectDriveInfo(DEFAULT_TARGET_SERIAL);

    if (opts.json) {
        OutputJson(info);
    } else {
        OutputText(info);
    }

    return EXIT_SUCCESS;
}

} // namespace commands
} // namespace hdd
