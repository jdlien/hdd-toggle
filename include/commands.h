#pragma once
// Command entry points for HDD Toggle CLI

#ifndef HDD_COMMANDS_H
#define HDD_COMMANDS_H

#include <windows.h>

namespace hdd {
namespace commands {

// Relay command: Control USB relay power
// Usage: hdd-toggle relay <on|off>
//        hdd-toggle relay <1|2> <on|off>
int RunRelay(int argc, char* argv[]);

// Wake command: Power on and wake the drive
// Usage: hdd-toggle wake
int RunWake(int argc, char* argv[]);

// Sleep command: Safely eject and power off the drive
// Usage: hdd-toggle sleep [--offline]
int RunSleep(int argc, char* argv[]);

// Status command: Show current drive status
// Usage: hdd-toggle status [--json]
int RunStatus(int argc, char* argv[]);

// GUI command: Launch the system tray application
// Usage: hdd-toggle [gui]
int LaunchTrayApp(HINSTANCE hInstance);

// Help command: Show usage information
int ShowHelp();

// Version command: Show version information
int ShowVersion();

// Helper: Control relay power (used by wake/sleep)
// Simpler interface for internal use
bool ControlRelayPower(bool on);

} // namespace commands
} // namespace hdd

#endif // HDD_COMMANDS_H
