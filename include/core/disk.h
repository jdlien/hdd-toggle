#pragma once
// Disk utilities for HDD Toggle
// Shared functions for drive detection and status

#ifndef HDD_CORE_DISK_H
#define HDD_CORE_DISK_H

#include "hdd-utils.h"
#include <string>

namespace hdd {
namespace core {

// Drive information structure
struct DriveInfo {
    DriveState state = DriveState::Unknown;
    std::string serialNumber;
    std::string model;
    int diskNumber = -1;
    bool found = false;
};

// Detect drive information using WMI
// Queries MSFT_Disk for the target drive by serial number
DriveInfo DetectDriveInfo(const std::string& targetSerial);

// Check if the target disk is currently online
bool IsDiskOnline(const std::string& targetSerial, const std::string& targetModel);

// Check if a help flag was passed
inline bool IsHelpFlag(const char* arg) {
    return _stricmp(arg, "-h") == 0 ||
           _stricmp(arg, "--help") == 0 ||
           _stricmp(arg, "/?") == 0 ||
           _stricmp(arg, "-help") == 0;
}

} // namespace core
} // namespace hdd

#endif // HDD_CORE_DISK_H
