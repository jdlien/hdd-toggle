#pragma once
// HDD Control Utilities - Testable pure functions
// All functions in this header are pure (no side effects) and can be unit tested

#include <string>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace hdd {

//=============================================================================
// String Utilities
//=============================================================================

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

// Case-insensitive string comparison
inline bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// Check if string starts with prefix (case-sensitive)
inline bool StartsWith(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size()) return false;
    return str.compare(0, prefix.size(), prefix) == 0;
}

// Check if string ends with suffix (case-sensitive)
inline bool EndsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Convert string to lowercase
inline std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Convert string to uppercase
inline std::string ToUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

//=============================================================================
// Drive State
//=============================================================================

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

// Get tooltip text for tray icon
inline std::string GetTooltipText(DriveState state) {
    switch (state) {
        case DriveState::Online: return "HDD Status: Drive Online";
        case DriveState::Offline: return "HDD Status: Drive Offline";
        case DriveState::Transitioning: return "HDD Status: Drive Transitioning...";
        default: return "HDD Status: Drive Unknown";
    }
}

// Check if state allows wake action
inline bool CanWake(DriveState state) {
    return state == DriveState::Offline || state == DriveState::Unknown;
}

// Check if state allows sleep action
inline bool CanSleep(DriveState state) {
    return state == DriveState::Online;
}

// Check if state is transitioning (operations should be blocked)
inline bool IsTransitioning(DriveState state) {
    return state == DriveState::Transitioning;
}

// Get the appropriate menu action text for current state
inline const char* GetPrimaryActionText(DriveState state) {
    if (CanSleep(state)) return "Sleep Drive";
    return "Wake Drive";
}

//=============================================================================
// Animation
//=============================================================================

// Animation dot patterns for progress indication
inline const char* GetAnimationDots(int frame) {
    static const char* dots[] = {"", ".", "..", "..."};
    int index = frame % 4;
    if (index < 0) index = 0;
    return dots[index];
}

// Generate animated tooltip text
inline std::string GetAnimatedTooltip(int frame) {
    return std::string("HDD Control - Working") + GetAnimationDots(frame);
}

// Calculate next animation frame (wraps at 4)
inline int NextAnimationFrame(int current) {
    return (current + 1) % 4;
}

//=============================================================================
// Timing Utilities
//=============================================================================

// Check if enough time has passed since last action (debounce)
// times are in milliseconds
inline bool HasDebounceElapsed(uint64_t lastTime, uint64_t currentTime, uint64_t debounceMs) {
    // Handle wrap-around (though unlikely with 64-bit)
    if (currentTime < lastTime) return true;
    return (currentTime - lastTime) >= debounceMs;
}

// Convert minutes to milliseconds
inline uint64_t MinutesToMs(unsigned int minutes) {
    return static_cast<uint64_t>(minutes) * 60ULL * 1000ULL;
}

// Convert seconds to milliseconds
inline uint64_t SecondsToMs(unsigned int seconds) {
    return static_cast<uint64_t>(seconds) * 1000ULL;
}

// Menu toggle debounce check (default 200ms)
inline bool ShouldShowMenu(uint64_t lastMenuCloseTime, uint64_t currentTime) {
    return HasDebounceElapsed(lastMenuCloseTime, currentTime, 200);
}

// Periodic check debounce (minimum 1 minute between checks)
inline bool ShouldPeriodicCheck(uint64_t lastCheckTime, uint64_t currentTime, bool isTransitioning) {
    if (isTransitioning) return false;
    return HasDebounceElapsed(lastCheckTime, currentTime, MinutesToMs(1));
}

//=============================================================================
// Configuration
//=============================================================================

// Configuration structure
struct Config {
    std::string targetSerial;
    std::string targetModel;
    std::string wakeCommand;
    std::string sleepCommand;
    unsigned int periodicCheckMinutes;
    unsigned int postOperationCheckSeconds;
    bool showNotifications;
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
        , debugMode(false)
    {}
};

// Validate periodic check minutes (minimum 1)
inline unsigned int ValidatePeriodicCheckMinutes(unsigned int value) {
    return value < 1 ? 1 : value;
}

// Validate post operation check seconds (minimum 1)
inline unsigned int ValidatePostOperationSeconds(unsigned int value) {
    return value < 1 ? 1 : value;
}

// Check if a serial number matches the target (case-insensitive, whitespace-trimmed)
inline bool SerialMatches(const std::string& actual, const std::string& target) {
    return EqualsIgnoreCase(TrimWhitespace(actual), TrimWhitespace(target));
}

//=============================================================================
// Path Utilities
//=============================================================================

// Get file extension (lowercase, without dot)
inline std::string GetExtension(const std::string& path) {
    size_t dotPos = path.rfind('.');
    size_t slashPos = path.rfind('/');
    size_t backslashPos = path.rfind('\\');

    // Find last path separator
    size_t lastSep = std::string::npos;
    if (slashPos != std::string::npos && backslashPos != std::string::npos) {
        lastSep = std::max(slashPos, backslashPos);
    } else if (slashPos != std::string::npos) {
        lastSep = slashPos;
    } else if (backslashPos != std::string::npos) {
        lastSep = backslashPos;
    }

    // Dot must exist and be after last separator
    if (dotPos == std::string::npos) return "";
    if (lastSep != std::string::npos && dotPos < lastSep) return "";

    return ToLower(path.substr(dotPos + 1));
}

// Get filename from path (without directory)
inline std::string GetFilename(const std::string& path) {
    size_t slashPos = path.rfind('/');
    size_t backslashPos = path.rfind('\\');

    size_t lastSep = std::string::npos;
    if (slashPos != std::string::npos && backslashPos != std::string::npos) {
        lastSep = std::max(slashPos, backslashPos);
    } else if (slashPos != std::string::npos) {
        lastSep = slashPos;
    } else if (backslashPos != std::string::npos) {
        lastSep = backslashPos;
    }

    if (lastSep == std::string::npos) return path;
    return path.substr(lastSep + 1);
}

// Check if path has an executable extension
inline bool IsExecutable(const std::string& path) {
    std::string ext = GetExtension(path);
    return ext == "exe" || ext == "bat" || ext == "cmd" || ext == "ps1";
}

// Join path components with proper separator
inline std::string JoinPath(const std::string& base, const std::string& name) {
    if (base.empty()) return name;
    if (name.empty()) return base;

    char lastChar = base.back();
    if (lastChar == '/' || lastChar == '\\') {
        return base + name;
    }
    return base + "\\" + name;
}

//=============================================================================
// Notification Messages
//=============================================================================

// Get notification message for operation completion
inline const char* GetCompletionMessage(bool isWake, bool success) {
    if (isWake) {
        return success ? "Drive wake completed" : "Drive wake failed";
    }
    // Sleep operation
    return success ? "Drive shutdown completed" : "Drive shutdown failed";
}

// Get notification message for operation start
inline const char* GetStartMessage(bool isWake) {
    return isWake ? "Waking drive..." : "Sleeping drive...";
}

} // namespace hdd
