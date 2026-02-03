#pragma once
// HDD Toggle - Main header with version and common types
// Single unified binary for HDD power control

#ifndef HDD_TOGGLE_H
#define HDD_TOGGLE_H

#include <string>

namespace hdd {

//=============================================================================
// Version Information
//=============================================================================

constexpr int VERSION_MAJOR = 3;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 1;

inline std::string GetVersionString() {
    return std::to_string(VERSION_MAJOR) + "." +
           std::to_string(VERSION_MINOR) + "." +
           std::to_string(VERSION_PATCH);
}

//=============================================================================
// Application Constants
//=============================================================================

// Application name and identifiers
constexpr const char* APP_NAME = "HDD Toggle";
constexpr const char* APP_INTERNAL_NAME = "hdd-toggle";
constexpr const wchar_t* APP_AUMID = L"HDDToggle.TrayApp.1";

// Default target drive (can be overridden in config)
constexpr const char* DEFAULT_TARGET_SERIAL = "2VH7TM9L";
constexpr const char* DEFAULT_TARGET_MODEL = "WDC WD181KFGX-68AFPN0";

// USB Relay identifiers
constexpr unsigned short RELAY_VENDOR_ID = 0x16C0;
constexpr unsigned short RELAY_PRODUCT_ID = 0x05DF;
constexpr int RELAY_REPORT_SIZE = 9;

//=============================================================================
// Command Types
//=============================================================================

enum class Command {
    GUI,        // Launch tray application (default)
    Wake,       // Wake the drive
    Sleep,      // Sleep the drive
    Relay,      // Control relay directly
    Status,     // Show drive status
    Help,       // Show help
    Version     // Show version
};

//=============================================================================
// Exit Codes
//=============================================================================

// Note: EXIT_SUCCESS and EXIT_FAILURE are defined in <cstdlib>
constexpr int EXIT_INVALID_ARGS = 2;
constexpr int EXIT_DEVICE_NOT_FOUND = 3;
constexpr int EXIT_OPERATION_FAILED = 4;

} // namespace hdd

#endif // HDD_TOGGLE_H
