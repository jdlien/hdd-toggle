#pragma once
// HDD Control Utilities - Testable pure functions

#include <string>
#include <cstring>

namespace hdd {

// Trim whitespace from both ends of a C string (in-place)
// Returns pointer to the trimmed start
inline char* TrimWhitespace(char* str) {
    if (!str || !*str) return str;

    // Trim leading
    char* start = str;
    while (*start && (*start == ' ' || *start == '\t')) start++;

    // Trim trailing (only if there's content left)
    size_t len = strlen(start);
    if (len > 0) {
        char* end = start + len - 1;
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
            *end-- = '\0';
        }
        // Handle single whitespace character
        if (end == start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
            *end = '\0';
        }
    }

    return start;
}

// Trim whitespace from a std::string
inline std::string TrimWhitespace(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";

    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// Drive states
enum class DriveState {
    Unknown = 0,
    Online = 1,
    Offline = 2,
    Transitioning = 3
};

// Get display string for drive state
inline const char* DriveStateToString(DriveState state) {
    switch (state) {
        case DriveState::Online: return "Drive Online";
        case DriveState::Offline: return "Drive Offline";
        case DriveState::Transitioning: return "Transitioning...";
        default: return "Unknown";
    }
}

// Get status string for menu display
inline const char* DriveStateToStatusString(DriveState state) {
    switch (state) {
        case DriveState::Online: return "Status: Drive Online";
        case DriveState::Offline: return "Status: Drive Offline";
        case DriveState::Transitioning: return "Status: Transitioning...";
        default: return "Status: Unknown";
    }
}

// Configuration structure
struct Config {
    std::string targetSerial;
    std::string targetModel;
    std::string wakeCommand;
    std::string sleepCommand;
    unsigned int periodicCheckMinutes;
    unsigned int postOperationCheckSeconds;
    bool showNotifications;
    unsigned int clickDebounceSeconds;
    bool debugMode;

    // Default values
    Config()
        : targetSerial("2VH7TM9L")
        , targetModel("WDC WD181KFGX-68AFPN0")
        , wakeCommand("wake-hdd.exe")
        , sleepCommand("sleep-hdd.exe")
        , periodicCheckMinutes(10)
        , postOperationCheckSeconds(3)
        , showNotifications(true)
        , clickDebounceSeconds(2)
        , debugMode(false)
    {}
};

} // namespace hdd
