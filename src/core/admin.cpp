// Administrator privilege utilities for HDD Toggle

#include "core/admin.h"
#include "core/process.h"
#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace hdd {
namespace core {

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    HANDLE token = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);

        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
            isAdmin = elevation.TokenIsElevated;
        }
        CloseHandle(token);
    }

    return isAdmin != FALSE;
}

bool RequestElevation() {
    if (IsRunningAsAdmin()) {
        return false; // Already admin
    }

    std::string exePath = GetExePath();

    // Get command line arguments (skip program name)
    LPWSTR cmdLine = GetCommandLineW();

    // Use ShellExecute with "runas" verb to request elevation
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = nullptr; // Will be set from exePath
    sei.lpParameters = cmdLine;
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    // Convert path to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, exePath.c_str(), -1, NULL, 0);
    if (wideLen > 0) {
        wchar_t* widePath = new wchar_t[wideLen];
        MultiByteToWideChar(CP_UTF8, 0, exePath.c_str(), -1, widePath, wideLen);
        sei.lpFile = widePath;

        BOOL result = ShellExecuteExW(&sei);
        delete[] widePath;

        return result != FALSE;
    }

    return false;
}

} // namespace core
} // namespace hdd
