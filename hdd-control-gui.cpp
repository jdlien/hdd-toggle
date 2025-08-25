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
#define IDT_STATUS_TIMER 2001
#define TRAY_ICON_ID 1
#define IDI_MAIN_ICON 101

// Drive states - must be defined before use
typedef enum {
    DS_UNKNOWN = 0,
    DS_ONLINE = 1,
    DS_OFFLINE = 2,
    DS_TRANSITIONING = 3
} DriveState;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CreateTrayIcon(HWND hwnd);
void RemoveTrayIcon();
void ShowContextMenu(HWND hwnd);
void UpdateTrayIcon();
DriveState DetectDriveState();
int ExecuteCommand(const char* command, BOOL hide_window);
void ShowBalloonTip(const char* title, const char* text, DWORD icon);
void OnWakeDrive();
void OnSleepDrive();
void OnRefreshStatus();

// Global variables
HINSTANCE g_hInst;
HWND g_hWnd;
NOTIFYICONDATA g_nid;
HMENU g_hMenu;
DriveState g_driveState = DS_UNKNOWN;
BOOL g_isTransitioning = FALSE;

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
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Create system tray icon
BOOL CreateTrayIcon(HWND hwnd) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = TRAY_ICON_ID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    
    // Load our custom HDD icon
    // Method 1: Try loading from file in same directory as exe
    char iconPath[MAX_PATH];
    char exeDir[MAX_PATH];
    GetModuleFileName(NULL, exeDir, MAX_PATH);
    // Remove the exe filename to get directory
    char* lastSlash = strrchr(exeDir, '\\');
    if (lastSlash) *lastSlash = '\0';
    sprintf_s(iconPath, MAX_PATH, "%s\\hdd-icon.ico", exeDir);
    
    g_nid.hIcon = (HICON)LoadImage(NULL, 
                                    iconPath, 
                                    IMAGE_ICON, 
                                    0, 0, 
                                    LR_LOADFROMFILE | LR_DEFAULTSIZE);
    
    if (!g_nid.hIcon) {
        // Method 2: Load from resources embedded in the exe - try main icon first
        g_nid.hIcon = (HICON)LoadImage(g_hInst, 
                                        MAKEINTRESOURCE(1), // Try main application icon first
                                        IMAGE_ICON, 
                                        0, 0, 
                                        LR_DEFAULTSIZE | LR_SHARED);
    }
    
    if (!g_nid.hIcon) {
        // Method 2b: Try the numbered icon resource
        g_nid.hIcon = (HICON)LoadImage(g_hInst, 
                                        MAKEINTRESOURCE(101), // Use direct numeric ID
                                        IMAGE_ICON, 
                                        0, 0, 
                                        LR_DEFAULTSIZE | LR_SHARED);
    }
    
    if (!g_nid.hIcon) {
        // Method 3: Try extracting icon from our own exe
        char exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);
        ExtractIconEx(exePath, 0, NULL, &g_nid.hIcon, 1);
    }
    
    if (!g_nid.hIcon) {
        // Method 4: Last fallback to system icon
        g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), "HDD Control - Checking status...");

    return Shell_NotifyIcon(NIM_ADD, &g_nid);
}

// Remove tray icon
void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
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
    TrackPopupMenu(g_hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
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
    
    // Keep the icon flag to preserve the icon when updating
    g_nid.uFlags = NIF_TIP | NIF_ICON;
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
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
    
    return foundTarget ? state : DS_UNKNOWN;
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

// Show balloon notification (working approach with system icons)
void ShowBalloonTip(const char* title, const char* text, DWORD icon) {
    g_nid.uFlags = NIF_INFO | NIF_ICON;
    g_nid.dwInfoFlags = icon;  // Use the passed icon type (NIIF_INFO, NIIF_WARNING, etc.)
    strcpy_s(g_nid.szInfoTitle, sizeof(g_nid.szInfoTitle), "");  // Remove redundant "HDD Control" title
    strcpy_s(g_nid.szInfo, sizeof(g_nid.szInfo), text);
    g_nid.uTimeout = 3000;
    
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
    
    // Reset flags but keep icon
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

// Wake drive action
void OnWakeDrive() {
    if (g_isTransitioning) return;
    
    g_isTransitioning = TRUE;
    g_driveState = DS_TRANSITIONING;
    UpdateTrayIcon();
    ShowBalloonTip("", "Waking drive...", NIIF_INFO);
    
    int result = ExecuteCommand("wake-hdd.exe", TRUE);
    
    if (result == 0) {
        ShowBalloonTip("", "Drive wake completed", NIIF_INFO);
    } else {
        ShowBalloonTip("", "Drive wake failed", NIIF_WARNING);
    }
    
    // Set a one-time timer to check status after wake operation
    SetTimer(g_hWnd, IDT_STATUS_TIMER, 3000, NULL);
}

// Sleep drive action
void OnSleepDrive() {
    if (g_isTransitioning) return;
    
    g_isTransitioning = TRUE;
    g_driveState = DS_TRANSITIONING;
    UpdateTrayIcon();
    ShowBalloonTip("", "Sleeping drive...", NIIF_INFO);
    
    int result = ExecuteCommand("sleep-hdd.exe", TRUE);
    
    if (result == 0) {
        ShowBalloonTip("", "Drive sleep completed", NIIF_INFO);
    } else {
        ShowBalloonTip("", "Drive sleep failed", NIIF_WARNING);
    }
    
    // Set a one-time timer to check status after sleep operation
    SetTimer(g_hWnd, IDT_STATUS_TIMER, 3000, NULL);
}

// Refresh status action
void OnRefreshStatus() {
    g_driveState = DetectDriveState();
    UpdateTrayIcon();
    ShowBalloonTip("", "Status refreshed", NIIF_INFO);
}

// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Create mutex to ensure single instance
    HANDLE hMutex = CreateMutex(NULL, TRUE, "Global\\HDDControlGUI_SingleInstance");
    
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
    
    // Register window class with our icon
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));  // Set main app icon
    wc.lpszClassName = "HDDControlTray";
    
    if (!RegisterClass(&wc)) {
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