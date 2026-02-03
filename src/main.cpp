// HDD Toggle - Unified Binary Entry Point
// Command router that dispatches to appropriate subcommand
//
// Usage:
//   hdd-toggle                     # GUI (default)
//   hdd-toggle gui                 # GUI (explicit)
//   hdd-toggle wake                # Wake drive
//   hdd-toggle sleep [--offline]   # Sleep drive
//   hdd-toggle relay <on|off>      # Control all relays
//   hdd-toggle relay <1|2> <on|off># Control single relay
//   hdd-toggle status [--json]     # Drive status
//   hdd-toggle --help              # Help
//   hdd-toggle --version           # Version

#include "hdd-toggle.h"
#include "commands.h"
#include <windows.h>
#include <cstdio>
#include <cstring>

namespace {

// Attach to parent console for CLI output when run from cmd/PowerShell
// Returns true if console was attached
bool AttachParentConsole() {
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        freopen_s(&dummy, "CONIN$", "r", stdin);
        return true;
    }
    return false;
}

// Allocate a new console for CLI commands when no parent console exists
bool AllocateConsole() {
    if (AllocConsole()) {
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        freopen_s(&dummy, "CONIN$", "r", stdin);
        return true;
    }
    return false;
}

// Parse command from arguments
hdd::Command ParseCommand(int argc, char* argv[]) {
    if (argc < 2) {
        return hdd::Command::GUI;
    }

    const char* cmd = argv[1];

    // Global flags first
    if (_stricmp(cmd, "--help") == 0 || _stricmp(cmd, "-h") == 0 || _stricmp(cmd, "/?") == 0) {
        return hdd::Command::Help;
    }
    if (_stricmp(cmd, "--version") == 0 || _stricmp(cmd, "-v") == 0) {
        return hdd::Command::Version;
    }

    // Subcommands
    if (_stricmp(cmd, "gui") == 0) return hdd::Command::GUI;
    if (_stricmp(cmd, "wake") == 0) return hdd::Command::Wake;
    if (_stricmp(cmd, "sleep") == 0) return hdd::Command::Sleep;
    if (_stricmp(cmd, "relay") == 0) return hdd::Command::Relay;
    if (_stricmp(cmd, "status") == 0) return hdd::Command::Status;
    if (_stricmp(cmd, "help") == 0) return hdd::Command::Help;
    if (_stricmp(cmd, "version") == 0) return hdd::Command::Version;

    // Unknown command - show help
    return hdd::Command::Help;
}

} // anonymous namespace

namespace hdd {
namespace commands {

int ShowHelp() {
    printf("%s v%s - Hard Drive Power Control\n\n", APP_NAME, GetVersionString().c_str());
    printf("Usage: hdd-toggle [command] [options]\n\n");
    printf("Commands:\n");
    printf("  (none), gui    Launch system tray application (default)\n");
    printf("  wake           Power on and initialize the drive\n");
    printf("  sleep          Safely eject and power off the drive\n");
    printf("  relay          Control USB relay directly\n");
    printf("  status         Show current drive status\n");
    printf("  help           Show this help message\n");
    printf("  version        Show version information\n\n");
    printf("Examples:\n");
    printf("  hdd-toggle                    Launch tray app\n");
    printf("  hdd-toggle wake               Wake the drive\n");
    printf("  hdd-toggle sleep --offline    Sleep with offline flag\n");
    printf("  hdd-toggle relay on           Turn on all relays\n");
    printf("  hdd-toggle relay 1 off        Turn off relay 1\n");
    printf("  hdd-toggle status --json      Get status as JSON\n\n");
    printf("For command-specific help, use: hdd-toggle <command> --help\n");
    return EXIT_SUCCESS;
}

int ShowVersion() {
    printf("%s v%s\n", APP_NAME, GetVersionString().c_str());
    return EXIT_SUCCESS;
}

} // namespace commands
} // namespace hdd

// Main entry point
int main(int argc, char* argv[]) {
    hdd::Command cmd = ParseCommand(argc, argv);

    // GUI mode doesn't need console
    if (cmd == hdd::Command::GUI) {
        return hdd::commands::LaunchTrayApp(GetModuleHandle(NULL));
    }

    // CLI commands need console output
    bool hasConsole = AttachParentConsole();
    if (!hasConsole) {
        // For commands that produce output but have no console, allocate one
        // This handles cases like double-clicking hdd-toggle.exe with args from a shortcut
        AllocateConsole();
    }

    // Calculate subcommand args (skip program name and command name)
    int subArgc = (argc > 2) ? argc - 2 : 0;
    char** subArgv = (argc > 2) ? &argv[2] : nullptr;

    int result;
    switch (cmd) {
        case hdd::Command::Wake:
            result = hdd::commands::RunWake(subArgc, subArgv);
            break;

        case hdd::Command::Sleep:
            result = hdd::commands::RunSleep(subArgc, subArgv);
            break;

        case hdd::Command::Relay:
            result = hdd::commands::RunRelay(subArgc, subArgv);
            break;

        case hdd::Command::Status:
            result = hdd::commands::RunStatus(subArgc, subArgv);
            break;

        case hdd::Command::Version:
            result = hdd::commands::ShowVersion();
            break;

        case hdd::Command::Help:
        default:
            result = hdd::commands::ShowHelp();
            break;
    }

    // If we allocated a console, wait for keypress before closing
    // This helps when running from a shortcut or file explorer
    if (!hasConsole && (cmd != hdd::Command::Status || result != EXIT_SUCCESS)) {
        printf("\nPress any key to exit...\n");
        getchar();
    }

    return result;
}

// WinMain entry point for Windows GUI mode (no console window)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    return main(__argc, __argv);
}
