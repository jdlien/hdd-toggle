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

#pragma comment(lib, "shell32.lib")
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

// Drive states - must be defined before use
typedef enum {
    DS_UNKNOWN = 0,
    DS_ONLINE = 1,
    DS_OFFLINE = 2,
    DS_TRANSITIONING = 3
} DriveState;

// Async operation data structure
struct AsyncOperationData {
    HWND hwnd;
    BOOL isWake;  // TRUE for wake, FALSE for sleep
};

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
DWORD WINAPI AsyncDriveOperation(LPVOID lpParam);
DWORD WINAPI AsyncPeriodicCheck(LPVOID lpParam);
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
HFONT g_hBoldMenuFont = NULL;  // Bold font for menu headers
DriveState g_driveState = DS_UNKNOWN;
BOOL g_isTransitioning = FALSE;
char g_statusMenuText[64] = "";  // Store status text for owner-drawn menu
UINT WM_TASKBARCREATED = 0;  // Will be set by RegisterWindowMessage
UINT_PTR g_animationTimer = 0;
int g_animationFrame = 0;
ULONGLONG g_lastPeriodicCheck = 0;
ULONGLONG g_lastClickTime = 0;
AppConfig g_config = {0};

// Ensure bold menu font is created
static void EnsureBoldMenuFont(HWND hwnd) {
    if (g_hBoldMenuFont) return;

    NONCLIENTMETRICS ncm = { sizeof(ncm) };
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

    LOGFONT lf = ncm.lfMenuFont;
    lf.lfWeight = FW_BOLD;
    lf.lfQuality = CLEARTYPE_NATURAL_QUALITY;

    g_hBoldMenuFont = CreateFontIndirect(&lf);
}

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
                    {
                        // Left-click: refresh status with configurable debouncing
                        ULONGLONG now = GetTickCount64();
                        ULONGLONG debounceMs = g_config.clickDebounceSeconds * 1000;
                        if (now - g_lastClickTime >= debounceMs) {
                            g_lastClickTime = now;
                            OnRefreshStatus();
                        }
                        // Ignore rapid clicks to prevent toast spam
                    }
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
                CreateThread(NULL, 0, AsyncPeriodicCheck, (LPVOID)hwnd, 0, NULL);
            }
            break;

        case WM_MEASUREITEM:
            {
                LPMEASUREITEMSTRUCT mis = (LPMEASUREITEMSTRUCT)lParam;
                if (mis->CtlType == ODT_MENU && mis->itemID == IDM_STATUS_DISPLAY) {
                    HDC hdc = GetDC(hwnd);
                    HFONT hOldFont = (HFONT)SelectObject(hdc, g_hBoldMenuFont);

                    LPCSTR txt = (LPCSTR)mis->itemData;
                    SIZE sz = {0};
                    GetTextExtentPoint32(hdc, txt, lstrlen(txt), &sz);

                    UINT dpi = GetDpiForWindow(hwnd);
                    int padX = MulDiv(12, dpi, 96);
                    int padY = MulDiv(0, dpi, 96);

                    mis->itemWidth = sz.cx + padX * 2;
                    mis->itemHeight = sz.cy + padY * 2;

                    SelectObject(hdc, hOldFont);
                    ReleaseDC(hwnd, hdc);
                    return TRUE;
                }
            }
            break;

        case WM_DRAWITEM:
            {
                LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
                if (dis->CtlType == ODT_MENU && dis->itemID == IDM_STATUS_DISPLAY) {
                    // Fill background
                    FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_MENU));
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SetTextColor(dis->hDC, GetSysColor(COLOR_MENUTEXT));

                    HFONT hOldFont = (HFONT)SelectObject(dis->hDC, g_hBoldMenuFont);

                    UINT dpi = GetDpiForWindow(g_hWnd);
                    int padX = MulDiv(18, dpi, 96);
                    RECT r = dis->rcItem;
                    r.left += padX;

                    DrawText(dis->hDC, (LPCSTR)dis->itemData, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

                    SelectObject(dis->hDC, hOldFont);
                    return TRUE;
                }
            }
            break;

        case WM_DESTROY:
            RemoveTrayIcon();
            if (g_hMenu) {
                DestroyMenu(g_hMenu);
            }
            if (g_hBoldMenuFont) {
                DeleteObject(g_hBoldMenuFont);
                g_hBoldMenuFont = NULL;
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
        char iconPath[MAX_PATH];
        char exeDir[MAX_PATH];
        GetModuleFileName(NULL, exeDir, MAX_PATH);
        char* lastSlash = strrchr(exeDir, '\\');
        if (lastSlash) *lastSlash = '\0';
        sprintf_s(iconPath, MAX_PATH, "%s\\hdd-icon.ico", exeDir);

        g_nid.hIcon = (HICON)LoadImage(NULL,
                                       iconPath,
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

    // Ensure bold font exists
    EnsureBoldMenuFont(hwnd);

    // Add status display at the top (bold heading that can't be clicked)
    if (g_driveState == DS_ONLINE) {
        strcpy_s(g_statusMenuText, sizeof(g_statusMenuText), "Drive Online");
    } else if (g_driveState == DS_OFFLINE) {
        strcpy_s(g_statusMenuText, sizeof(g_statusMenuText), "Drive Offline");
    } else if (g_driveState == DS_TRANSITIONING) {
        strcpy_s(g_statusMenuText, sizeof(g_statusMenuText), "Drive Transitioning...");
    } else {
        strcpy_s(g_statusMenuText, sizeof(g_statusMenuText), "Drive Status Unknown");
    }
    // Use MF_DISABLED to prevent interaction, pass text as itemData
    AppendMenu(g_hMenu, MF_OWNERDRAW | MF_DISABLED, IDM_STATUS_DISPLAY, (LPCSTR)g_statusMenuText);
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
    char iniPath[MAX_PATH];
    char exeDir[MAX_PATH];

    GetModuleFileName(NULL, exeDir, MAX_PATH);
    char* lastSlash = strrchr(exeDir, '\\');
    if (lastSlash) *lastSlash = '\0';
    sprintf_s(iniPath, MAX_PATH, "%s\\hdd-control.ini", exeDir);

    // Check if INI already exists
    if (PathFileExists(iniPath)) {
        return; // Don't overwrite existing file
    }

    // Use FILE* approach for better compatibility
    FILE* file = NULL;
    if (fopen_s(&file, iniPath, "w") != 0 || file == NULL) {
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
    char iniPath[MAX_PATH];
    char exeDir[MAX_PATH];
    char buffer[256];

    GetModuleFileName(NULL, exeDir, MAX_PATH);
    char* lastSlash = strrchr(exeDir, '\\');
    if (lastSlash) *lastSlash = '\0';
    sprintf_s(iniPath, MAX_PATH, "%s\\hdd-control.ini", exeDir);

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
    BOOL iniExists = PathFileExists(iniPath);
    if (!iniExists) {
        CreateDefaultIniFile();
        return; // Use defaults after creation attempt
    }

    // Read values from INI file
    GetPrivateProfileString("Drive", "SerialNumber", g_config.targetSerial,
                           g_config.targetSerial, sizeof(g_config.targetSerial), iniPath);
    GetPrivateProfileString("Drive", "Model", g_config.targetModel,
                           g_config.targetModel, sizeof(g_config.targetModel), iniPath);

    GetPrivateProfileString("Commands", "WakeCommand", g_config.wakeCommand,
                           g_config.wakeCommand, sizeof(g_config.wakeCommand), iniPath);
    GetPrivateProfileString("Commands", "SleepCommand", g_config.sleepCommand,
                           g_config.sleepCommand, sizeof(g_config.sleepCommand), iniPath);

    // Read timing values
    DWORD checkMinutes = GetPrivateProfileInt("Timing", "PeriodicCheckMinutes", g_config.periodicCheckMinutes, iniPath);
    if (checkMinutes >= 1) { // Minimum 1 minute
        g_config.periodicCheckMinutes = checkMinutes;
    }
    g_config.postOperationCheckSeconds = GetPrivateProfileInt("Timing", "PostOperationCheckSeconds",
                                                             g_config.postOperationCheckSeconds, iniPath);

    // Read UI settings
    g_config.showNotifications = GetPrivateProfileInt("UI", "ShowNotifications", g_config.showNotifications, iniPath);
    g_config.clickDebounceSeconds = GetPrivateProfileInt("UI", "ClickDebounceSeconds", g_config.clickDebounceSeconds, iniPath);

    // Read advanced settings
    g_config.debugMode = GetPrivateProfileInt("Advanced", "DebugMode", g_config.debugMode, iniPath);
}

// Async periodic drive check function
DWORD WINAPI AsyncPeriodicCheck(LPVOID lpParam) {
    HWND hwnd = (HWND)lpParam;

    // Avoid checking during operations or too frequently
    ULONGLONG now = GetTickCount64();
    if (g_isTransitioning || (now - g_lastPeriodicCheck < 60000)) {
        return 0; // Skip if operation in progress or checked within last minute
    }

    g_lastPeriodicCheck = now;
    DriveState newState = DetectDriveState();

    // Only update if state actually changed
    if (newState != g_driveState) {
        // Post message to main thread for UI update
        PostMessage(hwnd, WM_COMMAND, IDM_REFRESH_STATUS, 0);
    }

    return 0;
}

// WMI-based disk detection using COM - completely silent, no windows!
DriveState DetectDriveState() {
    HRESULT hres;
    IWbemLocator *pLoc = NULL;
    IWbemServices *pSvc = NULL;
    IEnumWbemClassObject* pEnumerator = NULL;
    IWbemClassObject *pclsObj[10];
    ULONG uReturn = 0;
    DriveState state = DS_UNKNOWN;
    BOOL foundTarget = FALSE;

    // Initialize COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        return DS_UNKNOWN;
    }

    // Set COM security levels
    hres = CoInitializeSecurity(
        NULL,
        -1,                          // COM authentication
        NULL,                        // Authentication services
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_NONE,      // Default authentication
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
        NULL,                        // Authentication info
        EOAC_NONE,                   // Additional capabilities
        NULL                         // Reserved
    );

    // Obtain the initial locator to WMI
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID *) &pLoc);

    if (FAILED(hres)) {
        CoUninitialize();
        return DS_UNKNOWN;
    }

    // Connect to WMI through the IWbemLocator::ConnectServer method
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\Microsoft\\Windows\\Storage"), // Object path of WMI namespace
        NULL,                    // User name. NULL = current user
        NULL,                    // User password. NULL = current
        0,                       // Locale. NULL indicates current
        NULL,                    // Security flags.
        0,                       // Authority (for example, Kerberos)
        0,                       // Context object
        &pSvc                    // pointer to IWbemServices proxy
    );

    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        return DS_UNKNOWN;
    }

    // Set security levels on the proxy
    hres = CoSetProxyBlanket(
        pSvc,                        // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
        NULL,                        // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
        NULL,                        // client identity
        EOAC_NONE                    // proxy capabilities
    );

    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return DS_UNKNOWN;
    }

    // Use the IWbemServices pointer to make requests of WMI
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSFT_Disk"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);

    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return DS_UNKNOWN;
    }

    // Get the data from the query
    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, pclsObj, &uReturn);

        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;

        // Get SerialNumber
        hr = pclsObj[0]->Get(L"SerialNumber", 0, &vtProp, 0, 0);
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
                hr = pclsObj[0]->Get(L"IsOffline", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr) && vtProp.vt == VT_BOOL) {
                    if (vtProp.boolVal == VARIANT_TRUE) {
                        state = DS_OFFLINE;
                    } else {
                        state = DS_ONLINE;
                    }
                }
                VariantClear(&vtProp);
                pclsObj[0]->Release();
                break;
            }
        }
        VariantClear(&vtProp);
        pclsObj[0]->Release();
    }

    // Cleanup
    if (pEnumerator) pEnumerator->Release();
    if (pSvc) pSvc->Release();
    if (pLoc) pLoc->Release();
    CoUninitialize();

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
DWORD WINAPI AsyncDriveOperation(LPVOID lpParam) {
    AsyncOperationData* data = (AsyncOperationData*)lpParam;

    const char* exe = data->isWake ? g_config.wakeCommand : g_config.sleepCommand;

    // Execute the command
    int result = ExecuteCommand(exe, TRUE);

    // Post message back to main thread for UI updates
    PostMessage(data->hwnd, WM_COMMAND,
                data->isWake ? IDM_WAKE_COMPLETE : IDM_SLEEP_COMPLETE,
                (LPARAM)result);

    delete data;
    return 0;
}

// Wake drive action
void OnWakeDrive() {
    if (g_isTransitioning) return;

    g_isTransitioning = TRUE;
    g_driveState = DS_TRANSITIONING;
    UpdateTrayIcon();
    ShowBalloonTip("", "Waking drive...", NIIF_INFO);
    StartProgressAnimation();

    // Create thread for async operation
    AsyncOperationData* data = new AsyncOperationData{g_hWnd, TRUE};
    CreateThread(NULL, 0, AsyncDriveOperation, data, 0, NULL);
}

// Sleep drive action
void OnSleepDrive() {
    if (g_isTransitioning) return;

    g_isTransitioning = TRUE;
    g_driveState = DS_TRANSITIONING;
    UpdateTrayIcon();
    ShowBalloonTip("", "Sleeping drive...", NIIF_INFO);
    StartProgressAnimation();

    // Create thread for async operation
    AsyncOperationData* data = new AsyncOperationData{g_hWnd, FALSE};
    CreateThread(NULL, 0, AsyncDriveOperation, data, 0, NULL);
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