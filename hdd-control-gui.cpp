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
#include <shobjidl.h>  // For SetCurrentProcessExplicitAppUserModelID
#include <shlobj.h>  // For SHGetFolderPathW, CSIDL_PROGRAMS
#include <propvarutil.h>  // For InitPropVariantFromString
#include <propkey.h>  // For PKEY_AppUserModel_ID
#pragma comment(lib, "propsys.lib")
#include <thread>
#include <string>
#include <filesystem>

// C++/WinRT for toast notifications (Windows 11)
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>
#pragma comment(lib, "windowsapp")

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

// Application state - encapsulates all runtime state
struct AppState {
    // Windows handles
    HINSTANCE hInstance = nullptr;
    HWND hWnd = nullptr;
    HMENU hMenu = nullptr;

    // Tray icon
    NOTIFYICONDATA nid = {};

    // Drive state
    DriveState driveState = DS_UNKNOWN;
    bool isTransitioning = false;

    // Animation
    UINT_PTR animationTimer = 0;
    int animationFrame = 0;

    // Timing
    ULONGLONG lastPeriodicCheck = 0;
    ULONGLONG lastClickTime = 0;
    ULONGLONG lastMenuCloseTime = 0;  // For menu toggle behavior

    // Configuration
    AppConfig config = {};

    // Windows messages
    UINT wmTaskbarCreated = 0;
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
BOOL EnsureStartMenuShortcut();

// AUMID for toast notifications
const wchar_t* AUMID = L"HDDControl.TrayApp.1";

// Global application state
AppState g_app;

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
            g_app.driveState = DetectDriveState();
            UpdateTrayIcon();
            // Start periodic check timer (configurable minutes)
            SetTimer(hwnd, IDT_PERIODIC_CHECK, g_app.config.periodicCheckMinutes * 60000, NULL);
            break;

        case WM_TRAYICON:
            switch (LOWORD(lParam)) {
                case WM_LBUTTONUP:
                case WM_RBUTTONUP:
                case WM_CONTEXTMENU:
                    // Toggle behavior: ignore click if menu just closed (within 200ms)
                    if (GetTickCount64() - g_app.lastMenuCloseTime > 200) {
                        ShowContextMenu(hwnd);
                    }
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
                    SetTimer(hwnd, IDT_STATUS_TIMER, g_app.config.postOperationCheckSeconds * 1000, NULL);
                    break;
                case IDM_SLEEP_COMPLETE:
                    StopProgressAnimation();
                    if (lParam == 0) {
                        ShowBalloonTip("", "Drive shutdown completed", NIIF_INFO);
                    } else {
                        ShowBalloonTip("", "Drive shutdown failed", NIIF_WARNING);
                    }
                    SetTimer(hwnd, IDT_STATUS_TIMER, g_app.config.postOperationCheckSeconds * 1000, NULL);
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
                if (newState != g_app.driveState) {
                    g_app.driveState = newState;
                    UpdateTrayIcon();
                }
                // Stop the timer after checking - no continuous polling
                KillTimer(hwnd, IDT_STATUS_TIMER);
                g_app.isTransitioning = FALSE;
            } else if (wParam == IDT_ANIMATION_TIMER) {
                // Animate tooltip text with dots
                char tooltip[128];
                const char* dots[] = {"", ".", "..", "..."};
                g_app.animationFrame = (g_app.animationFrame + 1) % 4;

                sprintf_s(tooltip, sizeof(tooltip), "HDD Control - Working%s",
                          dots[g_app.animationFrame]);

                strcpy_s(g_app.nid.szTip, sizeof(g_app.nid.szTip), tooltip);
                g_app.nid.uFlags = NIF_TIP;
                Shell_NotifyIcon(NIM_MODIFY, &g_app.nid);
            } else if (wParam == IDT_PERIODIC_CHECK) {
                // Run periodic check in background thread
                std::thread(AsyncPeriodicCheck, hwnd).detach();
            }
            break;

        case WM_DESTROY:
            RemoveTrayIcon();
            if (g_app.hMenu) {
                DestroyMenu(g_app.hMenu);
            }
            KillTimer(hwnd, IDT_STATUS_TIMER);
            KillTimer(hwnd, IDT_PERIODIC_CHECK);
            PostQuitMessage(0);
            break;

        default:
            // Handle TaskbarCreated message (Explorer restart)
            if (uMsg == g_app.wmTaskbarCreated) {
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
    ZeroMemory(&g_app.nid, sizeof(g_app.nid));
    g_app.nid.cbSize = sizeof(g_app.nid);
    g_app.nid.hWnd = hwnd;
    g_app.nid.uID = TRAY_ICON_ID;
    g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_app.nid.uCallbackMessage = WM_TRAYICON;

    // Load icon with proper size for system tray (SM_CXSMICON x SM_CYSMICON)
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);

    // Method 1: Load from resources with proper ID and size
    g_app.nid.hIcon = (HICON)LoadImage(g_app.hInstance,
                                   MAKEINTRESOURCE(IDI_MAIN_ICON),
                                   IMAGE_ICON,
                                   cx, cy,
                                   LR_DEFAULTCOLOR);

    // Method 2: Fallback to legacy resource IDs if main fails
    if (!g_app.nid.hIcon) {
        g_app.nid.hIcon = (HICON)LoadImage(g_app.hInstance,
                                       MAKEINTRESOURCE(1),
                                       IMAGE_ICON,
                                       cx, cy,
                                       LR_DEFAULTCOLOR);
    }

    // Method 3: Try loading from external file as last resort
    if (!g_app.nid.hIcon) {
        fs::path iconPath = GetExeDirectory() / "hdd-icon.ico";
        g_app.nid.hIcon = (HICON)LoadImage(NULL,
                                       iconPath.string().c_str(),
                                       IMAGE_ICON,
                                       cx, cy,
                                       LR_LOADFROMFILE | LR_DEFAULTCOLOR);
    }

    // Method 4: System fallback
    if (!g_app.nid.hIcon) {
        g_app.nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    strcpy_s(g_app.nid.szTip, sizeof(g_app.nid.szTip), "HDD Control - Checking status...");

    // Add the tray icon with retry logic for startup timing
    // Explorer's notification area may not be ready immediately at logon
    const int MAX_RETRIES = 10;
    const int RETRY_DELAY_MS = 1000;

    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        if (Shell_NotifyIcon(NIM_ADD, &g_app.nid)) {
            return TRUE;
        }
        // Wait before retrying - Explorer may still be initializing
        Sleep(RETRY_DELAY_MS);
    }

    return FALSE;
}

// Remove tray icon
void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &g_app.nid);
    if (g_app.nid.hIcon) {
        DestroyIcon(g_app.nid.hIcon);
        g_app.nid.hIcon = NULL;
    }
}

// Show context menu
void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    // Always destroy and recreate the menu to ensure correct state
    if (g_app.hMenu) {
        DestroyMenu(g_app.hMenu);
    }

    g_app.hMenu = CreatePopupMenu();

    // Add status display at the top as disabled text (allows Windows 11 modern menu styling)
    const char* statusText;
    if (g_app.driveState == DS_ONLINE) {
        statusText = "Status: Drive Online";
    } else if (g_app.driveState == DS_OFFLINE) {
        statusText = "Status: Drive Offline";
    } else if (g_app.driveState == DS_TRANSITIONING) {
        statusText = "Status: Transitioning...";
    } else {
        statusText = "Status: Unknown";
    }
    AppendMenu(g_app.hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, IDM_STATUS_DISPLAY, statusText);
    AppendMenu(g_app.hMenu, MF_SEPARATOR, 0, NULL);

    // Add appropriate action based on current drive state
    if (g_app.driveState == DS_ONLINE) {
        AppendMenu(g_app.hMenu, MF_STRING, IDM_SLEEP_DRIVE, "Sleep Drive");
    } else {
        AppendMenu(g_app.hMenu, MF_STRING, IDM_WAKE_DRIVE, "Wake Drive");
    }

    AppendMenu(g_app.hMenu, MF_STRING, IDM_REFRESH_STATUS, "Refresh Status");
    AppendMenu(g_app.hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(g_app.hMenu, MF_STRING, IDM_EXIT, "Exit");

    // Disable actions during transition
    if (g_app.isTransitioning) {
        EnableMenuItem(g_app.hMenu, (g_app.driveState == DS_ONLINE) ? IDM_SLEEP_DRIVE : IDM_WAKE_DRIVE, MF_GRAYED);
    }

    SetForegroundWindow(hwnd);
    // Use proper menu positioning that stays within screen bounds
    TrackPopupMenu(g_app.hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);

    // Record when menu closed for toggle behavior
    g_app.lastMenuCloseTime = GetTickCount64();
}

// Update tray icon and tooltip based on current state
void UpdateTrayIcon() {
    switch (g_app.driveState) {
        case DS_ONLINE:
            strcpy_s(g_app.nid.szTip, sizeof(g_app.nid.szTip), "HDD Status: Drive Online");
            break;
        case DS_OFFLINE:
            strcpy_s(g_app.nid.szTip, sizeof(g_app.nid.szTip), "HDD Status: Drive Offline");
            break;
        case DS_TRANSITIONING:
            strcpy_s(g_app.nid.szTip, sizeof(g_app.nid.szTip), "HDD Status: Drive Transitioning...");
            break;
        default:
            strcpy_s(g_app.nid.szTip, sizeof(g_app.nid.szTip), "HDD Status: Drive Unknown");
            break;
    }

    // Load the appropriate icon for the current state
    HICON newIcon = LoadIconForDriveState(g_app.driveState);
    if (newIcon) {
        if (g_app.nid.hIcon) {
            DestroyIcon(g_app.nid.hIcon);
        }
        g_app.nid.hIcon = newIcon;
    }

    // Keep the icon flag to preserve the icon when updating
    g_app.nid.uFlags = NIF_TIP | NIF_ICON;  // Remove NIF_GUID for now
    Shell_NotifyIcon(NIM_MODIFY, &g_app.nid);
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

    return (HICON)LoadImage(g_app.hInstance,
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
    strcpy_s(g_app.config.targetSerial, sizeof(g_app.config.targetSerial), "2VH7TM9L");
    strcpy_s(g_app.config.targetModel, sizeof(g_app.config.targetModel), "WDC WD181KFGX-68AFPN0");
    strcpy_s(g_app.config.wakeCommand, sizeof(g_app.config.wakeCommand), "wake-hdd.exe");
    strcpy_s(g_app.config.sleepCommand, sizeof(g_app.config.sleepCommand), "sleep-hdd.exe");
    g_app.config.periodicCheckMinutes = 10;
    g_app.config.postOperationCheckSeconds = 3;
    g_app.config.showNotifications = TRUE;
    g_app.config.clickDebounceSeconds = 2;
    g_app.config.startMinimized = TRUE;
    g_app.config.checkElevation = TRUE;
    g_app.config.wmiTimeout = 30000;
    g_app.config.threadTimeout = 60000;
    g_app.config.maxRetries = 3;
    g_app.config.debugMode = FALSE;

    // Always try to create default INI if it doesn't exist
    if (!fs::exists(iniPath)) {
        CreateDefaultIniFile();
        return; // Use defaults after creation attempt
    }

    // Read values from INI file
    GetPrivateProfileString("Drive", "SerialNumber", g_app.config.targetSerial,
                           g_app.config.targetSerial, sizeof(g_app.config.targetSerial), iniPathC);
    GetPrivateProfileString("Drive", "Model", g_app.config.targetModel,
                           g_app.config.targetModel, sizeof(g_app.config.targetModel), iniPathC);

    GetPrivateProfileString("Commands", "WakeCommand", g_app.config.wakeCommand,
                           g_app.config.wakeCommand, sizeof(g_app.config.wakeCommand), iniPathC);
    GetPrivateProfileString("Commands", "SleepCommand", g_app.config.sleepCommand,
                           g_app.config.sleepCommand, sizeof(g_app.config.sleepCommand), iniPathC);

    // Read timing values
    DWORD checkMinutes = GetPrivateProfileInt("Timing", "PeriodicCheckMinutes", g_app.config.periodicCheckMinutes, iniPathC);
    if (checkMinutes >= 1) { // Minimum 1 minute
        g_app.config.periodicCheckMinutes = checkMinutes;
    }
    g_app.config.postOperationCheckSeconds = GetPrivateProfileInt("Timing", "PostOperationCheckSeconds",
                                                             g_app.config.postOperationCheckSeconds, iniPathC);

    // Read UI settings
    g_app.config.showNotifications = GetPrivateProfileInt("UI", "ShowNotifications", g_app.config.showNotifications, iniPathC);
    g_app.config.clickDebounceSeconds = GetPrivateProfileInt("UI", "ClickDebounceSeconds", g_app.config.clickDebounceSeconds, iniPathC);

    // Read advanced settings
    g_app.config.debugMode = GetPrivateProfileInt("Advanced", "DebugMode", g_app.config.debugMode, iniPathC);
}

// Async periodic drive check function
void AsyncPeriodicCheck(HWND hwnd) {
    // Avoid checking during operations or too frequently
    ULONGLONG now = GetTickCount64();
    if (g_app.isTransitioning || (now - g_app.lastPeriodicCheck < 60000)) {
        return; // Skip if operation in progress or checked within last minute
    }

    g_app.lastPeriodicCheck = now;
    DriveState newState = DetectDriveState();

    // Only update if state actually changed
    if (newState != g_app.driveState) {
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

            if (strcmp(start, g_app.config.targetSerial) == 0) {
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

// Show toast notification using WinRT (Windows 11 modern notifications)
// Falls back to legacy balloon if toast fails
void ShowBalloonTip(const char* title, const char* text, DWORD icon) {
    using namespace winrt::Windows::UI::Notifications;
    using namespace winrt::Windows::Data::Xml::Dom;

    try {
        // Initialize WinRT on this thread if needed
        static bool winrtInitialized = false;
        if (!winrtInitialized) {
            winrt::init_apartment();
            winrtInitialized = true;
        }

        // Convert to wide string
        wchar_t wtext[256];
        MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 256);

        // Build toast XML
        std::wstring xml = L"<toast duration=\"short\"><visual><binding template=\"ToastGeneric\">";
        xml += L"<text>" + std::wstring(wtext) + L"</text>";
        xml += L"</binding></visual></toast>";

        XmlDocument doc;
        doc.LoadXml(xml);

        ToastNotification toast(doc);
        toast.ExpiresOnReboot(true);  // Transient - won't persist in Action Center

        auto notifier = ToastNotificationManager::CreateToastNotifier(AUMID);
        notifier.Show(toast);
    } catch (...) {
        // Fallback to legacy balloon notification
        g_app.nid.uFlags = NIF_INFO;
        g_app.nid.dwInfoFlags = icon;
        strcpy_s(g_app.nid.szInfoTitle, sizeof(g_app.nid.szInfoTitle), "");
        strcpy_s(g_app.nid.szInfo, sizeof(g_app.nid.szInfo), text);
        g_app.nid.uTimeout = 3000;
        Shell_NotifyIcon(NIM_MODIFY, &g_app.nid);
        g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    }
}

// Animation functions for progress feedback
void StartProgressAnimation() {
    g_app.animationFrame = 0;
    g_app.animationTimer = SetTimer(g_app.hWnd, IDT_ANIMATION_TIMER, 500, NULL);  // 500ms animation speed
}

void StopProgressAnimation() {
    if (g_app.animationTimer) {
        KillTimer(g_app.hWnd, IDT_ANIMATION_TIMER);
        g_app.animationTimer = 0;
    }
    UpdateTrayIcon();  // Restore normal icon
}

// Async drive operation thread function
void AsyncDriveOperation(HWND hwnd, bool isWake) {
    const char* exe = isWake ? g_app.config.wakeCommand : g_app.config.sleepCommand;

    // Execute the command
    int result = ExecuteCommand(exe, TRUE);

    // Post message back to main thread for UI updates
    PostMessage(hwnd, WM_COMMAND,
                isWake ? IDM_WAKE_COMPLETE : IDM_SLEEP_COMPLETE,
                (LPARAM)result);
}

// Wake drive action
void OnWakeDrive() {
    if (g_app.isTransitioning) return;

    g_app.isTransitioning = TRUE;
    g_app.driveState = DS_TRANSITIONING;
    UpdateTrayIcon();
    ShowBalloonTip("", "Waking drive...", NIIF_INFO);
    StartProgressAnimation();

    // Run async operation in background thread
    std::thread(AsyncDriveOperation, g_app.hWnd, true).detach();
}

// Sleep drive action
void OnSleepDrive() {
    if (g_app.isTransitioning) return;

    g_app.isTransitioning = TRUE;
    g_app.driveState = DS_TRANSITIONING;
    UpdateTrayIcon();
    ShowBalloonTip("", "Sleeping drive...", NIIF_INFO);
    StartProgressAnimation();

    // Run async operation in background thread
    std::thread(AsyncDriveOperation, g_app.hWnd, false).detach();
}

// Refresh status action
void OnRefreshStatus() {
    g_app.driveState = DetectDriveState();
    UpdateTrayIcon();

    // Show actual drive status in notification
    const char* statusMessage;
    if (g_app.driveState == DS_ONLINE) {
        statusMessage = "Drive Online";
    } else if (g_app.driveState == DS_OFFLINE) {
        statusMessage = "Drive Offline";
    } else if (g_app.driveState == DS_TRANSITIONING) {
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

// Create Start Menu shortcut with AUMID for toast notifications
// Toast notifications require a shortcut with AppUserModelID property set
BOOL EnsureStartMenuShortcut() {
    // Get the Start Menu Programs path
    wchar_t startMenuPath[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, startMenuPath))) {
        return FALSE;
    }

    // Build shortcut path
    std::wstring shortcutPath = std::wstring(startMenuPath) + L"\\HDD Control.lnk";

    // Check if shortcut already exists
    if (GetFileAttributesW(shortcutPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return TRUE;  // Already exists
    }

    // Get our executable path
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // Initialize COM for this operation
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return FALSE;
    }

    BOOL success = FALSE;
    IShellLinkW* pShellLink = nullptr;
    IPersistFile* pPersistFile = nullptr;
    IPropertyStore* pPropertyStore = nullptr;

    // Create IShellLink instance
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          IID_IShellLinkW, (void**)&pShellLink);
    if (FAILED(hr)) goto cleanup;

    // Set shortcut properties
    pShellLink->SetPath(exePath);
    pShellLink->SetWorkingDirectory(GetExeDirectory().wstring().c_str());
    pShellLink->SetDescription(L"HDD Control - System tray app for hard drive power management");

    // Get IPropertyStore to set AUMID
    hr = pShellLink->QueryInterface(IID_IPropertyStore, (void**)&pPropertyStore);
    if (FAILED(hr)) goto cleanup;

    // Set the AppUserModelID property
    {
        PROPVARIANT propVar;
        hr = InitPropVariantFromString(AUMID, &propVar);
        if (SUCCEEDED(hr)) {
            hr = pPropertyStore->SetValue(PKEY_AppUserModel_ID, propVar);
            PropVariantClear(&propVar);
        }
        if (FAILED(hr)) goto cleanup;

        hr = pPropertyStore->Commit();
        if (FAILED(hr)) goto cleanup;
    }

    // Save the shortcut
    hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
    if (FAILED(hr)) goto cleanup;

    hr = pPersistFile->Save(shortcutPath.c_str(), TRUE);
    success = SUCCEEDED(hr);

cleanup:
    if (pPersistFile) pPersistFile->Release();
    if (pPropertyStore) pPropertyStore->Release();
    if (pShellLink) pShellLink->Release();
    CoUninitialize();

    return success;
}


// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize DPI awareness for modern scaling
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize Windows 11 dark mode support (must be before window creation)
    InitDarkMode();

    // Set AUMID for toast notifications (no Microsoft registration needed)
    SetCurrentProcessExplicitAppUserModelID(AUMID);

    // Ensure Start Menu shortcut exists (required for toast notifications)
    EnsureStartMenuShortcut();

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

            // Show toast notification that instance is already running
            using namespace winrt::Windows::UI::Notifications;
            using namespace winrt::Windows::Data::Xml::Dom;
            try {
                std::wstring xml = L"<toast duration=\"short\"><visual><binding template=\"ToastGeneric\">";
                xml += L"<text>Application is already running</text>";
                xml += L"</binding></visual></toast>";

                XmlDocument doc;
                doc.LoadXml(xml);

                ToastNotification toast(doc);
                toast.ExpiresOnReboot(true);

                auto notifier = ToastNotificationManager::CreateToastNotifier(AUMID);
                notifier.Show(toast);
            } catch (...) {
                // Silently fail
            }
        }

        return 0;  // Exit silently
    }

    g_app.hInstance = hInstance;

    // Register TaskbarCreated message for Explorer restart handling
    g_app.wmTaskbarCreated = RegisterWindowMessage("TaskbarCreated");

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
    g_app.hWnd = CreateWindow("HDDControlTray", "HDD Control", 0, 0, 0, 0, 0,
                          NULL, NULL, hInstance, NULL);

    if (!g_app.hWnd) {
        MessageBox(NULL, "Failed to create window", "Error", MB_ICONERROR);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;
    }

    // Apply dark mode to our window for proper menu theming
    ApplyDarkModeToWindow(g_app.hWnd);

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