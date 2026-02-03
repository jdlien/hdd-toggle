#pragma once
// Process execution utilities for HDD Toggle

#ifndef HDD_CORE_PROCESS_H
#define HDD_CORE_PROCESS_H

#include <string>

namespace hdd {
namespace core {

// Execute a command and wait for completion
// Returns the exit code of the process
// If hideWindow is true, the process runs without a visible window
int ExecuteCommand(const std::string& command, bool hideWindow = true);

// Execute a command with captured output
// Returns the exit code, and fills output with stdout content
int ExecuteCommandWithOutput(const std::string& command, std::string& output, bool hideWindow = true);

// Get the directory containing the current executable
std::string GetExeDirectory();

// Get the full path to the current executable
std::string GetExePath();

// Find an executable on PATH or in current directory
// Returns empty string if not found
std::string FindExecutable(const std::string& name);

} // namespace core
} // namespace hdd

#endif // HDD_CORE_PROCESS_H
