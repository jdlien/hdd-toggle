// Process execution utilities for HDD Toggle

#include "core/process.h"
#include <windows.h>
#include <cstdlib>
#include <vector>

namespace hdd {
namespace core {

int ExecuteCommand(const std::string& command, bool hideWindow) {
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    DWORD exitCode = 1;

    si.cb = sizeof(si);
    if (hideWindow) {
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = NULL;
        si.hStdOutput = NULL;
        si.hStdError = NULL;
    }

    DWORD creationFlags = hideWindow ? (CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP) : 0;

    // CreateProcess needs a modifiable string
    std::vector<char> cmdBuf(command.begin(), command.end());
    cmdBuf.push_back('\0');

    if (CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, creationFlags, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return static_cast<int>(exitCode);
}

int ExecuteCommandWithOutput(const std::string& command, std::string& output, bool hideWindow) {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return 1;
    }

    // Ensure the read handle is not inherited
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    DWORD exitCode = 1;

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = hideWindow ? SW_HIDE : SW_SHOW;
    si.hStdInput = NULL;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    DWORD creationFlags = hideWindow ? CREATE_NO_WINDOW : 0;

    std::vector<char> cmdBuf(command.begin(), command.end());
    cmdBuf.push_back('\0');

    if (CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, creationFlags, NULL, NULL, &si, &pi)) {
        CloseHandle(hWritePipe);
        hWritePipe = NULL;

        // Read output
        output.clear();
        char buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    if (hWritePipe) CloseHandle(hWritePipe);
    CloseHandle(hReadPipe);

    return static_cast<int>(exitCode);
}

std::string GetExeDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);

    // Find last backslash
    std::string pathStr(path);
    size_t lastSlash = pathStr.rfind('\\');
    if (lastSlash != std::string::npos) {
        return pathStr.substr(0, lastSlash);
    }
    return pathStr;
}

std::string GetExePath() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return std::string(path);
}

std::string FindExecutable(const std::string& name) {
    // Try current directory first
    std::string exeDir = GetExeDirectory();
    std::string fullPath = exeDir + "\\" + name;
    if (GetFileAttributesA(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return fullPath;
    }

    // Try just the name (in case it's in working directory)
    if (GetFileAttributesA(name.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return name;
    }

    // Search in PATH
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return "";

    std::string pathStr(pathEnv);
    size_t start = 0;
    size_t end;

    while ((end = pathStr.find(';', start)) != std::string::npos || start < pathStr.length()) {
        if (end == std::string::npos) end = pathStr.length();

        std::string dir = pathStr.substr(start, end - start);
        if (!dir.empty()) {
            std::string testPath = dir + "\\" + name;
            if (GetFileAttributesA(testPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                return testPath;
            }
        }

        start = end + 1;
        if (start >= pathStr.length()) break;
    }

    return "";
}

} // namespace core
} // namespace hdd
