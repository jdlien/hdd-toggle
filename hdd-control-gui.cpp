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

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#define TARGET_SERIAL "2VH7TM9L"
#define TARGET_MODEL "WDC WD181KFGX-68AFPN0"
#define MAX_COMMAND_LEN 1024
#define WM_TRAYICON (WM_USER + 1)
#define IDM_WAKE_DRIVE 1001
#define IDM_SLEEP_DRIVE 1002
#define IDM_REFRESH_STATUS 1003
#define IDM_EXIT 1004
#define IDM_WAKE_COMPLETE 1005
#define IDM_SLEEP_COMPLETE 1006
#define IDT_STATUS_TIMER 2001
#define IDT_ANIMATION_TIMER 2002
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

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            if (!CreateTrayIcon(hwnd)) {
                return -1;
            }
            // Perform immediate initial detection
            g_driveState = DetectDriveState();
            UpdateTrayIcon();
            // No automatic timer - only check when needed
            break;

        case WM_TRAYICON:
            switch (LOWORD(lParam)) {
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
                    SetTimer(hwnd, IDT_STATUS_TIMER, 3000, NULL);
                    break;
                case IDM_SLEEP_COMPLETE:
                    StopProgressAnimation();
                    if (lParam == 0) {
                        ShowBalloonTip("", "Drive sleep completed", NIIF_INFO);
                    } else {
                        ShowBalloonTip("", "Drive sleep failed", NIIF_WARNING);
                    }
                    SetTimer(hwnd, IDT_STATUS_TIMER, 3000, NULL);
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
            }
            break;

        case WM_DESTROY:
            RemoveTrayIcon();
            if (g_hMenu) {
                DestroyMenu(g_hMenu);
            }
            KillTimer(hwnd, IDT_STATUS_TIMER);
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
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;  // Remove NIF_GUID temporarily
    g_nid.uCallbackMessage = WM_TRAYICON;
    // g_nid.uVersion = NOTIFYICON_VERSION_4;  // Comment out version for now
    
    // Temporarily disable GUID to test basic functionality
    // static const GUID trayIconGuid = 
    //     { 0x7d6f1a94, 0x6b0f, 0x46b4, { 0x9a, 0x6e, 0x3a, 0x21, 0x7b, 0x52, 0x41, 0x12 } };
    // g_nid.guidItem = trayIconGuid;
    
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

    // Add the tray icon
    BOOL result = Shell_NotifyIcon(NIM_ADD, &g_nid);
    
    // Skip version setting for now to test basic functionality
    // if (result) {
    //     Shell_NotifyIcon(NIM_SETVERSION, &g_nid);
    // }
    
    return result;
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
            strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), "HDD Control - Drive Online");
            break;
        case DS_OFFLINE:
            strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), "HDD Control - Drive Offline");
            break;
        case DS_TRANSITIONING:
            strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), "HDD Control - Transitioning...");
            break;
        default:
            strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), "HDD Control - Status Unknown");
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
            
            if (strcmp(start, TARGET_SERIAL) == 0) {
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
    g_nid.uTimeout = 3000;
    
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
    
    // Reset flags to original state
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;  // Remove NIF_GUID for now
}

// Animation functions for progress feedback
void StartProgressAnimation() {
    g_animationFrame = 0;
    g_animationTimer = SetTimer(g_hWnd, IDT_ANIMATION_TIMER, 500, NULL);
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
    
    const char* exe = data->isWake ? "wake-hdd.exe" : "sleep-hdd.exe";
    
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
    ShowBalloonTip("", "Status refreshed", NIIF_INFO);
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