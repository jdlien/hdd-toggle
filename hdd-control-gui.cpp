// HDD Control GUI - System Tray Application
// Controls mechanical hard drive power via relay and manages drive status

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commctrl.h>
#include <winioctl.h>
#include <setupapi.h>
#include <wbemidl.h>
#include <comdef.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <thread>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// Simple COM smart pointer template (like ComPtr but without WRL dependency)
template<typename T>
class ComPtr {
public:
    ComPtr() : m_ptr(nullptr) {}
    ~ComPtr() { Release(); }

    T** operator&() { return &m_ptr; }
    T* operator->() { return m_ptr; }
    T* Get() const { return m_ptr; }
    operator bool() const { return m_ptr != nullptr; }

    void Release() {
        if (m_ptr) {
            m_ptr->Release();
            m_ptr = nullptr;
        }
    }

private:
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    T* m_ptr;
};

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "gdi32.lib")

#define MAX_COMMAND_LEN 1024
#define WM_TRAYICON (WM_USER + 1)
#define IDM_WAKE_DRIVE 1001
#define IDM_SLEEP_DRIVE 1002
#define IDM_REFRESH_STATUS 1003
#define IDM_EXIT 1004
#define IDM_WAKE_COMPLETE 1005
#define IDM_SLEEP_COMPLETE 1006
#define IDM_STATUS_DISPLAY 1007
#define IDT_STATUS_TIMER 2001
#define IDT_ANIMATION_TIMER 2002
#define IDT_PERIODIC_CHECK 2003
#define TRAY_ICON_ID 1
#define IDI_MAIN_ICON 100
#define IDI_DRIVE_ON_ICON 101
#define IDI_DRIVE_OFF_ICON 102

// Windows 11 Dark Mode support (undocumented APIs from uxtheme.dll)
enum PreferredAppMode {
    PAM_Default = 0,
    PAM_AllowDark = 1,
    PAM_ForceDark = 2,
    PAM_ForceLight = 3,
    PAM_Max = 4
};

typedef BOOL (WINAPI *fnAllowDarkModeForWindow)(HWND hWnd, BOOL allow);
typedef PreferredAppMode (WINAPI *fnSetPreferredAppMode)(PreferredAppMode appMode);
typedef void (WINAPI *fnFlushMenuThemes)();
typedef BOOL (WINAPI *fnShouldAppsUseDarkMode)();

static fnAllowDarkModeForWindow pAllowDarkModeForWindow = nullptr;
static fnSetPreferredAppMode pSetPreferredAppMode = nullptr;
static fnFlushMenuThemes pFlushMenuThemes = nullptr;
static fnShouldAppsUseDarkMode pShouldAppsUseDarkMode = nullptr;

// Initialize dark mode - call before creating windows
void InitDarkMode() {
    HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUxtheme) {
        // Load undocumented functions by ordinal
        pAllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133));
        pSetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
        pFlushMenuThemes = (fnFlushMenuThemes)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136));
        pShouldAppsUseDarkMode = (fnShouldAppsUseDarkMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(132));

        if (pSetPreferredAppMode) {
            pSetPreferredAppMode(PAM_AllowDark);
        }
        if (pFlushMenuThemes) {
            pFlushMenuThemes();
        }
    }
}

// Apply dark mode to a specific window
void ApplyDarkModeToWindow(HWND hwnd) {
    if (pAllowDarkModeForWindow) {
        pAllowDarkModeForWindow(hwnd, TRUE);
    }
    // Also set the DWMWA_USE_IMMERSIVE_DARK_MODE attribute for title bar (if visible)
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));  // DWMWA_USE_IMMERSIVE_DARK_MODE
}

// Get the directory containing the executable
fs::path GetExeDirectory() {
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    return fs::path(exePath).parent_path();
}

// Drive states - must be defined before use
typedef enum {
    DS_UNKNOWN = 0,
    DS_ONLINE = 1,
    DS_OFFLINE = 2,
    DS_TRANSITIONING = 3
} DriveState;

// Configuration structure
struct AppConfig {
    // Drive identification
    char targetSerial[64];
    char targetModel[128];

    // Commands
    char wakeCommand[MAX_PATH];
    char sleepCommand[MAX_PATH];

    // Timing
    DWORD periodicCheckMinutes;
    DWORD postOperationCheckSeconds;

    // UI settings
    BOOL showNotifications;
    DWORD clickDebounceSeconds;

    // Startup
    BOOL startMinimized;
    BOOL checkElevation;

    // Advanced
    DWORD wmiTimeout;
    DWORD threadTimeout;
    DWORD maxRetries;
    BOOL debugMode;
};

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CreateTrayIcon(HWND hwnd);
void RemoveTrayIcon();
void ShowContextMenu(HWND hwnd);
void UpdateTrayIcon();
HICON LoadIconForDriveState(DriveState state);
DriveState DetectDriveState();
int ExecuteCommand(const char* command, BOOL hide_window);
void ShowBalloonTip(const char* title, const char* text, DWORD icon);
void StartProgressAnimation();
void StopProgressAnimation();
void AsyncDriveOperation(HWND hwnd, bool isWake);
void AsyncPeriodicCheck(HWND hwnd);
void LoadConfiguration();
void CreateDefaultIniFile();
void OnWakeDrive();
void OnSleepDrive();
void OnRefreshStatus();
BOOL IsRunningElevated();

// Global variables
HINSTANCE g_hInst;
HWND g_hWnd;
NOTIFYICONDATA g_nid;
HMENU g_hMenu;
DriveState g_driveState = DS_UNKNOWN;
BOOL g_isTransitioning = FALSE;
UINT WM_TASKBARCREATED = 0;  // Will be set by RegisterWindowMessage
UINT_PTR g_animationTimer = 0;
int g_animationFrame = 0;
ULONGLONG g_lastPeriodicCheck = 0;
ULONGLONG g_lastClickTime = 0;
AppConfig g_config = {0};

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            // Load configuration first
            LoadConfiguration();
            if (!CreateTrayIcon(hwnd)) {
                return -1;
            }
            // Perform immediate initial detection
            g_driveState = DetectDriveState();
            UpdateTrayIcon();
            // Start periodic check timer (configurable minutes)
            SetTimer(hwnd, IDT_PERIODIC_CHECK, g_config.periodicCheckMinutes * 60000, NULL);
            break;

        case WM_TRAYICON:
            switch (LOWORD(lParam)) {
                case WM_LBUTTONUP:
                    // Left-click: show context menu (same as right-click)
                    ShowContextMenu(hwnd);
                    break;
                case WM_RBUTTONUP:
                case WM_CONTEXTMENU:
                    ShowContextMenu(hwnd);
                    break;
            }
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_WAKE_DRIVE:
                    OnWakeDrive();
                    break;
                case IDM_SLEEP_DRIVE:
                    OnSleepDrive();
                    break;
                case IDM_REFRESH_STATUS:
                    OnRefreshStatus();
                    break;
                case IDM_EXIT:
                    PostQuitMessage(0);
                    break;
                case IDM_WAKE_COMPLETE:
                    StopProgressAnimation();
                    if (lParam == 0) {
                        ShowBalloonTip("", "Drive wake completed", NIIF_INFO);
                    } else {
                        ShowBalloonTip("", "Drive wake failed", NIIF_WARNING);
                    }
                    SetTimer(hwnd, IDT_STATUS_TIMER, g_config.postOperationCheckSeconds * 1000, NULL);
                    break;
                case IDM_SLEEP_COMPLETE:
                    StopProgressAnimation();
                    if (lParam == 0) {
                        ShowBalloonTip("", "Drive shutdown completed", NIIF_INFO);
                    } else {
                        ShowBalloonTip("", "Drive shutdown failed", NIIF_WARNING);
                    }
                    SetTimer(hwnd, IDT_STATUS_TIMER, g_config.postOperationCheckSeconds * 1000, NULL);
                    break;
                case IDM_STATUS_DISPLAY:
                    // Status display header - no action
                    break;
            }
            break;

        case WM_TIMER:
            if (wParam == IDT_STATUS_TIMER) {
                // Only used for checking after operations complete
                DriveState newState = DetectDriveState();
                if (newState != g_driveState) {
                    g_driveState = newState;
                    UpdateTrayIcon();
                }
                // Stop the timer after checking - no continuous polling
                KillTimer(hwnd, IDT_STATUS_TIMER);
                g_isTransitioning = FALSE;
            } else if (wParam == IDT_ANIMATION_TIMER) {
                // Animate tooltip text with dots
                char tooltip[128];
                const char* dots[] = {"", ".", "..", "..."};
                g_animationFrame = (g_animationFrame + 1) % 4;

                sprintf_s(tooltip, sizeof(tooltip), "HDD Control - Working%s",
                          dots[g_animationFrame]);

                strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), tooltip);
                g_nid.uFlags = NIF_TIP;
                Shell_NotifyIcon(NIM_MODIFY, &g_nid);
            } else if (wParam == IDT_PERIODIC_CHECK) {
                // Run periodic check in background thread
                std::thread(AsyncPeriodicCheck, hwnd).detach();
            }
            break;

        case WM_DESTROY:
            RemoveTrayIcon();
            if (g_hMenu) {
                DestroyMenu(g_hMenu);
            }
            KillTimer(hwnd, IDT_STATUS_TIMER);
            KillTimer(hwnd, IDT_PERIODIC_CHECK);
            PostQuitMessage(0);
            break;

        default:
            // Handle TaskbarCreated message (Explorer restart)
            if (uMsg == WM_TASKBARCREATED) {
                CreateTrayIcon(hwnd);
                UpdateTrayIcon();
                return 0;
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Create system tray icon with modern best practices
BOOL CreateTrayIcon(HWND hwnd) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = TRAY_ICON_ID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;

    // Load icon with proper size for system tray (SM_CXSMICON x SM_CYSMICON)
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);

    // Method 1: Load from resources with proper ID and size
    g_nid.hIcon = (HICON)LoadImage(g_hInst,
                                   MAKEINTRESOURCE(IDI_MAIN_ICON),
                                   IMAGE_ICON,
                                   cx, cy,
                                   LR_DEFAULTCOLOR);

    // Method 2: Fallback to legacy resource IDs if main fails
    if (!g_nid.hIcon) {
        g_nid.hIcon = (HICON)LoadImage(g_hInst,
                                       MAKEINTRESOURCE(1),
                                       IMAGE_ICON,
                                       cx, cy,
                                       LR_DEFAULTCOLOR);
    }

    // Method 3: Try loading from external file as last resort
    if (!g_nid.hIcon) {
        fs::path iconPath = GetExeDirectory() / "hdd-icon.ico";
        g_nid.hIcon = (HICON)LoadImage(NULL,
                                       iconPath.string().c_str(),
                                       IMAGE_ICON,
                                       cx, cy,
                                       LR_LOADFROMFILE | LR_DEFAULTCOLOR);
    }

    // Method 4: System fallback
    if (!g_nid.hIcon) {
        g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), "HDD Control - Checking status...");

    // Add the tray icon with retry logic for startup timing
    // Explorer's notification area may not be ready immediately at logon
    const int MAX_RETRIES = 10;
    const int RETRY_DELAY_MS = 1000;

    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        if (Shell_NotifyIcon(NIM_ADD, &g_nid)) {
            return TRUE;
        }
        // Wait before retrying - Explorer may still be initializing
        Sleep(RETRY_DELAY_MS);
    }

    return FALSE;
}

// Remove tray icon
void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
    if (g_nid.hIcon) {
        DestroyIcon(g_nid.hIcon);
        g_nid.hIcon = NULL;
    }
}

// Show context menu
void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    // Always destroy and recreate the menu to ensure correct state
    if (g_hMenu) {
        DestroyMenu(g_hMenu);
    }

    g_hMenu = CreatePopupMenu();

    // Add status display at the top as disabled text (allows Windows 11 modern menu styling)
    const char* statusText;
    if (g_driveState == DS_ONLINE) {
        statusText = "Status: Drive Online";
    } else if (g_driveState == DS_OFFLINE) {
        statusText = "Status: Drive Offline";
    } else if (g_driveState == DS_TRANSITIONING) {
        statusText = "Status: Transitioning...";
    } else {
        statusText = "Status: Unknown";
    }
    AppendMenu(g_hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, IDM_STATUS_DISPLAY, statusText);
    AppendMenu(g_hMenu, MF_SEPARATOR, 0, NULL);

    // Add appropriate action based on current drive state
    if (g_driveState == DS_ONLINE) {
        AppendMenu(g_hMenu, MF_STRING, IDM_SLEEP_DRIVE, "Sleep Drive");
    } else {
        AppendMenu(g_hMenu, MF_STRING, IDM_WAKE_DRIVE, "Wake Drive");
    }

    AppendMenu(g_hMenu, MF_STRING, IDM_REFRESH_STATUS, "Refresh Status");
    AppendMenu(g_hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(g_hMenu, MF_STRING, IDM_EXIT, "Exit");

    // Disable actions during transition
    if (g_isTransitioning) {
        EnableMenuItem(g_hMenu, (g_driveState == DS_ONLINE) ? IDM_SLEEP_DRIVE : IDM_WAKE_DRIVE, MF_GRAYED);
    }

    SetForegroundWindow(hwnd);
    // Use proper menu positioning that stays within screen bounds
    TrackPopupMenu(g_hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
}

// Update tray icon and tooltip based on current state
void UpdateTrayIcon() {
    switch (g_driveState) {
        case DS_ONLINE:
            strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), "HDD Status: Drive Online");
            break;
        case DS_OFFLINE:
            strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), "HDD Status: Drive Offline");
            break;
        case DS_TRANSITIONING:
            strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), "HDD Status: Drive Transitioning...");
            break;
        default:
            strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), "HDD Status: Drive Unknown");
            break;
    }

    // Load the appropriate icon for the current state
    HICON newIcon = LoadIconForDriveState(g_driveState);
    if (newIcon) {
        if (g_nid.hIcon) {
            DestroyIcon(g_nid.hIcon);
        }
        g_nid.hIcon = newIcon;
    }

    // Keep the icon flag to preserve the icon when updating
    g_nid.uFlags = NIF_TIP | NIF_ICON;  // Remove NIF_GUID for now
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
}

// Load the appropriate icon based on drive state
HICON LoadIconForDriveState(DriveState state) {
    int iconResource;

    switch (state) {
        case DS_ONLINE:
            iconResource = IDI_DRIVE_ON_ICON;
            break;
        case DS_OFFLINE:
            iconResource = IDI_DRIVE_OFF_ICON;
            break;
        case DS_TRANSITIONING:
        case DS_UNKNOWN:
        default:
            iconResource = IDI_MAIN_ICON;
            break;
    }

    // Load icon with proper size for system tray
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);

    return (HICON)LoadImage(g_hInst,
                           MAKEINTRESOURCE(iconResource),
                           IMAGE_ICON,
                           cx, cy,
                           LR_DEFAULTCOLOR);
}

// Create default INI file with example settings
void CreateDefaultIniFile() {
    fs::path iniPath = GetExeDirectory() / "hdd-control.ini";

    // Check if INI already exists
    if (fs::exists(iniPath)) {
        return; // Don't overwrite existing file
    }

    // Use FILE* approach for better compatibility
    FILE* file = NULL;
    if (fopen_s(&file, iniPath.string().c_str(), "w") != 0 || file == NULL) {
        return; // Couldn't create file - fail silently
    }

    const char* defaultIni =
        "# HDD Control Configuration File\n"
        "# Remove the '#' character to enable a setting. All values shown are defaults.\n"
        "\n"
        "[Drive]\n"
        "# Target drive identification - change these to match your specific drive\n"
        "#SerialNumber=2VH7TM9L\n"
        "#Model=WDC WD181KFGX-68AFPN0\n"
        "\n"
        "[Commands]\n"
        "# Paths to the wake and sleep executables (relative to hdd-control.exe)\n"
        "#WakeCommand=wake-hdd.exe\n"
        "#SleepCommand=sleep-hdd.exe\n"
        "\n"
        "[Timing]\n"
        "# Status check interval in minutes (minimum 1 minute)\n"
        "#PeriodicCheckMinutes=10\n"
        "# Status check delay after operations in seconds\n"
        "#PostOperationCheckSeconds=3\n"
        "\n"
        "[UI]\n"
        "# User interface settings\n"
        "#ShowNotifications=true\n"
        "# Click debounce delay in seconds to prevent toast spam\n"
        "#ClickDebounceSeconds=2\n"
        "\n"
        "[Advanced]\n"
        "# Advanced settings - modify with caution\n"
        "#DebugMode=false\n";

    fprintf(file, "%s", defaultIni);
    fclose(file);
}

// Load configuration from INI file with defaults
void LoadConfiguration() {
    fs::path iniPath = GetExeDirectory() / "hdd-control.ini";
    std::string iniPathStr = iniPath.string();
    const char* iniPathC = iniPathStr.c_str();

    // Set defaults first
    strcpy_s(g_config.targetSerial, sizeof(g_config.targetSerial), "2VH7TM9L");
    strcpy_s(g_config.targetModel, sizeof(g_config.targetModel), "WDC WD181KFGX-68AFPN0");
    strcpy_s(g_config.wakeCommand, sizeof(g_config.wakeCommand), "wake-hdd.exe");
    strcpy_s(g_config.sleepCommand, sizeof(g_config.sleepCommand), "sleep-hdd.exe");
    g_config.periodicCheckMinutes = 10;
    g_config.postOperationCheckSeconds = 3;
    g_config.showNotifications = TRUE;
    g_config.clickDebounceSeconds = 2;
    g_config.startMinimized = TRUE;
    g_config.checkElevation = TRUE;
    g_config.wmiTimeout = 30000;
    g_config.threadTimeout = 60000;
    g_config.maxRetries = 3;
    g_config.debugMode = FALSE;

    // Always try to create default INI if it doesn't exist
    if (!fs::exists(iniPath)) {
        CreateDefaultIniFile();
        return; // Use defaults after creation attempt
    }

    // Read values from INI file
    GetPrivateProfileString("Drive", "SerialNumber", g_config.targetSerial,
                           g_config.targetSerial, sizeof(g_config.targetSerial), iniPathC);
    GetPrivateProfileString("Drive", "Model", g_config.targetModel,
                           g_config.targetModel, sizeof(g_config.targetModel), iniPathC);

    GetPrivateProfileString("Commands", "WakeCommand", g_config.wakeCommand,
                           g_config.wakeCommand, sizeof(g_config.wakeCommand), iniPathC);
    GetPrivateProfileString("Commands", "SleepCommand", g_config.sleepCommand,
                           g_config.sleepCommand, sizeof(g_config.sleepCommand), iniPathC);

    // Read timing values
    DWORD checkMinutes = GetPrivateProfileInt("Timing", "PeriodicCheckMinutes", g_config.periodicCheckMinutes, iniPathC);
    if (checkMinutes >= 1) { // Minimum 1 minute
        g_config.periodicCheckMinutes = checkMinutes;
    }
    g_config.postOperationCheckSeconds = GetPrivateProfileInt("Timing", "PostOperationCheckSeconds",
                                                             g_config.postOperationCheckSeconds, iniPathC);

    // Read UI settings
    g_config.showNotifications = GetPrivateProfileInt("UI", "ShowNotifications", g_config.showNotifications, iniPathC);
    g_config.clickDebounceSeconds = GetPrivateProfileInt("UI", "ClickDebounceSeconds", g_config.clickDebounceSeconds, iniPathC);

    // Read advanced settings
    g_config.debugMode = GetPrivateProfileInt("Advanced", "DebugMode", g_config.debugMode, iniPathC);
}

// Async periodic drive check function
void AsyncPeriodicCheck(HWND hwnd) {
    // Avoid checking during operations or too frequently
    ULONGLONG now = GetTickCount64();
    if (g_isTransitioning || (now - g_lastPeriodicCheck < 60000)) {
        return; // Skip if operation in progress or checked within last minute
    }

    g_lastPeriodicCheck = now;
    DriveState newState = DetectDriveState();

    // Only update if state actually changed
    if (newState != g_driveState) {
        // Post message to main thread for UI update
        PostMessage(hwnd, WM_COMMAND, IDM_REFRESH_STATUS, 0);
    }
}

// RAII wrapper for COM initialization
class ComInitializer {
public:
    ComInitializer() : m_initialized(false) {
        HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
        m_initialized = SUCCEEDED(hr);
    }
    ~ComInitializer() {
        if (m_initialized) CoUninitialize();
    }
    bool IsInitialized() const { return m_initialized; }
private:
    bool m_initialized;
};

// WMI-based disk detection using COM with smart pointers
DriveState DetectDriveState() {
    DriveState state = DS_UNKNOWN;
    BOOL foundTarget = FALSE;

    // RAII COM initialization - automatically calls CoUninitialize on scope exit
    ComInitializer comInit;
    if (!comInit.IsInitialized()) {
        return DS_UNKNOWN;
    }

    // Set COM security levels
    CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_NONE,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL
    );

    // Smart pointers automatically release COM objects when they go out of scope
    ComPtr<IWbemLocator> pLoc;
    HRESULT hres = CoCreateInstance(
        CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) return DS_UNKNOWN;

    // Connect to WMI
    ComPtr<IWbemServices> pSvc;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\Microsoft\\Windows\\Storage"),
        NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) return DS_UNKNOWN;

    // Set security levels on the proxy
    hres = CoSetProxyBlanket(
        pSvc.Get(),
        RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE);
    if (FAILED(hres)) return DS_UNKNOWN;

    // Execute WMI query
    ComPtr<IEnumWbemClassObject> pEnumerator;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSFT_Disk"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);
    if (FAILED(hres)) return DS_UNKNOWN;

    // Iterate through results
    while (pEnumerator) {
        ComPtr<IWbemClassObject> pclsObj;
        ULONG uReturn = 0;
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (uReturn == 0) break;

        VARIANT vtProp;
        VariantInit(&vtProp);

        // Get SerialNumber
        hr = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
            _bstr_t serialNumber(vtProp.bstrVal, true);
            char serialStr[256];
            wcstombs_s(NULL, serialStr, 256, serialNumber, _TRUNCATE);

            // Trim whitespace
            char* start = serialStr;
            while (*start && (*start == ' ' || *start == '\t')) start++;
            char* end = start + strlen(start) - 1;
            while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
                *end-- = '\0';
            }

            if (strcmp(start, g_config.targetSerial) == 0) {
                foundTarget = TRUE;
                VariantClear(&vtProp);

                // Get IsOffline property
                hr = pclsObj->Get(L"IsOffline", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr) && vtProp.vt == VT_BOOL) {
                    state = (vtProp.boolVal == VARIANT_TRUE) ? DS_OFFLINE : DS_ONLINE;
                }
                VariantClear(&vtProp);
                break;
            }
        }
        VariantClear(&vtProp);
    }

    // No manual cleanup needed - ComPtr and ComInitializer handle it automatically
    return foundTarget ? state : DS_OFFLINE;
}

// Execute command and return exit code
int ExecuteCommand(const char* command, BOOL hide_window) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 1;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    if (hide_window) {
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = NULL;
        si.hStdOutput = NULL;
        si.hStdError = NULL;
    }

    DWORD creationFlags = hide_window ? (CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP) : 0;

    if (CreateProcess(NULL, (LPSTR)command, NULL, NULL, FALSE, creationFlags, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return exit_code;
}

// Show balloon notification (safe, reliable approach)
void ShowBalloonTip(const char* title, const char* text, DWORD icon) {
    g_nid.uFlags = NIF_INFO;
    g_nid.dwInfoFlags = icon;  // Use reliable system icons (NIIF_INFO, NIIF_WARNING, etc.)
    strcpy_s(g_nid.szInfoTitle, sizeof(g_nid.szInfoTitle), "");
    strcpy_s(g_nid.szInfo, sizeof(g_nid.szInfo), text);
    g_nid.uTimeout = 3000;  // 3 seconds (ignored by modern Windows)

    Shell_NotifyIcon(NIM_MODIFY, &g_nid);

    // Reset flags to original state
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;  // Remove NIF_GUID for now
}

// Animation functions for progress feedback
void StartProgressAnimation() {
    g_animationFrame = 0;
    g_animationTimer = SetTimer(g_hWnd, IDT_ANIMATION_TIMER, 500, NULL);  // 500ms animation speed
}

void StopProgressAnimation() {
    if (g_animationTimer) {
        KillTimer(g_hWnd, IDT_ANIMATION_TIMER);
        g_animationTimer = 0;
    }
    UpdateTrayIcon();  // Restore normal icon
}

// Async drive operation thread function
void AsyncDriveOperation(HWND hwnd, bool isWake) {
    const char* exe = isWake ? g_config.wakeCommand : g_config.sleepCommand;

    // Execute the command
    int result = ExecuteCommand(exe, TRUE);

    // Post message back to main thread for UI updates
    PostMessage(hwnd, WM_COMMAND,
                isWake ? IDM_WAKE_COMPLETE : IDM_SLEEP_COMPLETE,
                (LPARAM)result);
}

// Wake drive action
void OnWakeDrive() {
    if (g_isTransitioning) return;

    g_isTransitioning = TRUE;
    g_driveState = DS_TRANSITIONING;
    UpdateTrayIcon();
    ShowBalloonTip("", "Waking drive...", NIIF_INFO);
    StartProgressAnimation();

    // Run async operation in background thread
    std::thread(AsyncDriveOperation, g_hWnd, true).detach();
}

// Sleep drive action
void OnSleepDrive() {
    if (g_isTransitioning) return;

    g_isTransitioning = TRUE;
    g_driveState = DS_TRANSITIONING;
    UpdateTrayIcon();
    ShowBalloonTip("", "Sleeping drive...", NIIF_INFO);
    StartProgressAnimation();

    // Run async operation in background thread
    std::thread(AsyncDriveOperation, g_hWnd, false).detach();
}

// Refresh status action
void OnRefreshStatus() {
    g_driveState = DetectDriveState();
    UpdateTrayIcon();

    // Show actual drive status in notification
    const char* statusMessage;
    if (g_driveState == DS_ONLINE) {
        statusMessage = "Drive Online";
    } else if (g_driveState == DS_OFFLINE) {
        statusMessage = "Drive Offline";
    } else if (g_driveState == DS_TRANSITIONING) {
        statusMessage = "Drive Transitioning...";
    } else {
        statusMessage = "Drive Status Unknown";
    }
    ShowBalloonTip("", statusMessage, NIIF_INFO);
}

// Check if the current process is running with elevated privileges
BOOL IsRunningElevated() {
    BOOL isElevated = FALSE;
    HANDLE hToken = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD size;

        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }

    return isElevated;
}


// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize DPI awareness for modern scaling
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize Windows 11 dark mode support (must be before window creation)
    InitDarkMode();

    // Create mutex to ensure single instance
    HANDLE hMutex = CreateMutex(NULL, TRUE, "Global\\HDDControl_SingleInstance");

    if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is already running
        if (hMutex) {
            CloseHandle(hMutex);
        }

        // Find and bring existing instance to foreground (show its menu)
        HWND existingWindow = FindWindow("HDDControlTray", "HDD Control");
        if (existingWindow) {
            // Send a message to show the context menu
            PostMessage(existingWindow, WM_COMMAND, IDM_REFRESH_STATUS, 0);

            // Show a balloon tip that instance is already running
            NOTIFYICONDATA nid = {0};
            nid.cbSize = sizeof(nid);
            nid.hWnd = existingWindow;
            nid.uID = TRAY_ICON_ID;
            nid.uFlags = NIF_INFO;
            nid.dwInfoFlags = NIIF_INFO;  // Use standard info icon
            strcpy_s(nid.szInfoTitle, sizeof(nid.szInfoTitle), "HDD Control");
            strcpy_s(nid.szInfo, sizeof(nid.szInfo), "Application is already running");
            nid.uTimeout = 2000;
            Shell_NotifyIcon(NIM_MODIFY, &nid);
        }

        return 0;  // Exit silently
    }

    g_hInst = hInstance;

    // Register TaskbarCreated message for Explorer restart handling
    WM_TASKBARCREATED = RegisterWindowMessage("TaskbarCreated");

    // Register window class with proper icon support using WNDCLASSEX
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "HDDControlTray";

    // Load both large and small icons for complete coverage
    wc.hIcon = (HICON)LoadImage(hInstance,
                                MAKEINTRESOURCE(IDI_MAIN_ICON),
                                IMAGE_ICON,
                                GetSystemMetrics(SM_CXICON),
                                GetSystemMetrics(SM_CYICON),
                                LR_DEFAULTCOLOR);

    wc.hIconSm = (HICON)LoadImage(hInstance,
                                  MAKEINTRESOURCE(IDI_MAIN_ICON),
                                  IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON),
                                  LR_DEFAULTCOLOR);

    // Fallback to legacy resource IDs if main fails
    if (!wc.hIcon) {
        wc.hIcon = (HICON)LoadImage(hInstance,
                                    MAKEINTRESOURCE(1),
                                    IMAGE_ICON,
                                    GetSystemMetrics(SM_CXICON),
                                    GetSystemMetrics(SM_CYICON),
                                    LR_DEFAULTCOLOR);
    }

    if (!wc.hIconSm) {
        wc.hIconSm = (HICON)LoadImage(hInstance,
                                      MAKEINTRESOURCE(1),
                                      IMAGE_ICON,
                                      GetSystemMetrics(SM_CXSMICON),
                                      GetSystemMetrics(SM_CYSMICON),
                                      LR_DEFAULTCOLOR);
    }

    // Last fallback to system icons
    if (!wc.hIcon) {
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    if (!wc.hIconSm) {
        wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    }

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "Failed to register window class", "Error", MB_ICONERROR);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;
    }

    // Create hidden window for message handling
    g_hWnd = CreateWindow("HDDControlTray", "HDD Control", 0, 0, 0, 0, 0,
                          NULL, NULL, hInstance, NULL);

    if (!g_hWnd) {
        MessageBox(NULL, "Failed to create window", "Error", MB_ICONERROR);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;
    }

    // Apply dark mode to our window for proper menu theming
    ApplyDarkModeToWindow(g_hWnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Clean up mutex
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    return (int)msg.wParam;
}