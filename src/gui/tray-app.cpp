// HDD Toggle Tray Application
// System tray GUI for HDD power control
// Refactored from hdd-control-gui.cpp for unified binary

#include "commands.h"
#include "hdd-toggle.h"
#include "hdd-utils.h"
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <wbemidl.h>
#include <comdef.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <propvarutil.h>
#include <propkey.h>
#include <thread>
#include <string>
#include <filesystem>

// C++/WinRT for toast notifications
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>

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
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "windowsapp")

namespace fs = std::filesystem;

namespace hdd {
namespace commands {

namespace {

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

// Windows 11 Dark Mode support
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

static fnAllowDarkModeForWindow pAllowDarkModeForWindow = nullptr;
static fnSetPreferredAppMode pSetPreferredAppMode = nullptr;
static fnFlushMenuThemes pFlushMenuThemes = nullptr;

// Simple COM smart pointer template
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
        if (m_ptr) { m_ptr->Release(); m_ptr = nullptr; }
    }
private:
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    T* m_ptr;
};

// RAII COM initializer
class ComInitializer {
public:
    ComInitializer() : m_initialized(false) {
        HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
        m_initialized = SUCCEEDED(hr);
    }
    ~ComInitializer() { if (m_initialized) CoUninitialize(); }
    bool IsInitialized() const { return m_initialized; }
private:
    bool m_initialized;
};

// Configuration
struct AppConfig {
    char targetSerial[64];
    char targetModel[128];
    DWORD periodicCheckMinutes;
    DWORD postOperationCheckSeconds;
    BOOL showNotifications;
    BOOL debugMode;
};

// Application state
struct AppState {
    HINSTANCE hInstance = nullptr;
    HWND hWnd = nullptr;
    HMENU hMenu = nullptr;
    NOTIFYICONDATA nid = {};
    DriveState driveState = DriveState::Unknown;
    bool isTransitioning = false;
    UINT_PTR animationTimer = 0;
    int animationFrame = 0;
    ULONGLONG lastPeriodicCheck = 0;
    ULONGLONG lastMenuCloseTime = 0;
    AppConfig config = {};
    UINT wmTaskbarCreated = 0;
};

static AppState g_app;

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CreateTrayIcon(HWND hwnd);
void RemoveTrayIcon();
void ShowContextMenu(HWND hwnd);
void UpdateTrayIcon();
HICON LoadIconForDriveState(DriveState state);
DriveState DetectDriveState();
void ShowBalloonTip(const char* title, const char* text, DWORD icon);
void StartProgressAnimation();
void StopProgressAnimation();
void AsyncDriveOperation(HWND hwnd, bool isWake);
void AsyncPeriodicCheck(HWND hwnd);
void LoadConfiguration();
void OnWakeDrive();
void OnSleepDrive();
void OnRefreshStatus();
BOOL EnsureStartMenuShortcut();

// Get executable directory
fs::path GetExeDirectory() {
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    return fs::path(exePath).parent_path();
}

void InitDarkMode() {
    HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUxtheme) {
        pAllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133));
        pSetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
        pFlushMenuThemes = (fnFlushMenuThemes)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136));

        if (pSetPreferredAppMode) pSetPreferredAppMode(PAM_AllowDark);
        if (pFlushMenuThemes) pFlushMenuThemes();
    }
}

void ApplyDarkModeToWindow(HWND hwnd) {
    if (pAllowDarkModeForWindow) pAllowDarkModeForWindow(hwnd, TRUE);
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            LoadConfiguration();
            if (!CreateTrayIcon(hwnd)) return -1;
            g_app.driveState = DetectDriveState();
            UpdateTrayIcon();
            SetTimer(hwnd, IDT_PERIODIC_CHECK, g_app.config.periodicCheckMinutes * 60000, NULL);
            break;

        case WM_TRAYICON:
            switch (LOWORD(lParam)) {
                case WM_LBUTTONUP:
                case WM_RBUTTONUP:
                case WM_CONTEXTMENU:
                    if (GetTickCount64() - g_app.lastMenuCloseTime > 200) {
                        ShowContextMenu(hwnd);
                    }
                    break;
            }
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_WAKE_DRIVE: OnWakeDrive(); break;
                case IDM_SLEEP_DRIVE: OnSleepDrive(); break;
                case IDM_REFRESH_STATUS: OnRefreshStatus(); break;
                case IDM_EXIT: PostQuitMessage(0); break;
                case IDM_WAKE_COMPLETE:
                    StopProgressAnimation();
                    ShowBalloonTip("", lParam == 0 ? "Drive wake completed" : "Drive wake failed",
                                   lParam == 0 ? NIIF_INFO : NIIF_WARNING);
                    SetTimer(hwnd, IDT_STATUS_TIMER, g_app.config.postOperationCheckSeconds * 1000, NULL);
                    break;
                case IDM_SLEEP_COMPLETE:
                    StopProgressAnimation();
                    ShowBalloonTip("", lParam == 0 ? "Drive shutdown completed" : "Drive shutdown failed",
                                   lParam == 0 ? NIIF_INFO : NIIF_WARNING);
                    SetTimer(hwnd, IDT_STATUS_TIMER, g_app.config.postOperationCheckSeconds * 1000, NULL);
                    break;
            }
            break;

        case WM_TIMER:
            if (wParam == IDT_STATUS_TIMER) {
                DriveState newState = DetectDriveState();
                if (newState != g_app.driveState) {
                    g_app.driveState = newState;
                    UpdateTrayIcon();
                }
                KillTimer(hwnd, IDT_STATUS_TIMER);
                g_app.isTransitioning = false;
            } else if (wParam == IDT_ANIMATION_TIMER) {
                char tooltip[128];
                g_app.animationFrame = (g_app.animationFrame + 1) % 4;
                sprintf_s(tooltip, sizeof(tooltip), "HDD Toggle - Working%s", GetAnimationDots(g_app.animationFrame));
                strcpy_s(g_app.nid.szTip, sizeof(g_app.nid.szTip), tooltip);
                g_app.nid.uFlags = NIF_TIP;
                Shell_NotifyIcon(NIM_MODIFY, &g_app.nid);
            } else if (wParam == IDT_PERIODIC_CHECK) {
                std::thread(AsyncPeriodicCheck, hwnd).detach();
            }
            break;

        case WM_DESTROY:
            RemoveTrayIcon();
            if (g_app.hMenu) DestroyMenu(g_app.hMenu);
            KillTimer(hwnd, IDT_STATUS_TIMER);
            KillTimer(hwnd, IDT_PERIODIC_CHECK);
            PostQuitMessage(0);
            break;

        default:
            if (uMsg == g_app.wmTaskbarCreated) {
                CreateTrayIcon(hwnd);
                UpdateTrayIcon();
                return 0;
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

BOOL CreateTrayIcon(HWND hwnd) {
    ZeroMemory(&g_app.nid, sizeof(g_app.nid));
    g_app.nid.cbSize = sizeof(g_app.nid);
    g_app.nid.hWnd = hwnd;
    g_app.nid.uID = TRAY_ICON_ID;
    g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_app.nid.uCallbackMessage = WM_TRAYICON;

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);

    g_app.nid.hIcon = (HICON)LoadImage(g_app.hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON),
                                       IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
    if (!g_app.nid.hIcon) {
        g_app.nid.hIcon = (HICON)LoadImage(g_app.hInstance, MAKEINTRESOURCE(1),
                                           IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
    }
    if (!g_app.nid.hIcon) {
        fs::path iconPath = GetExeDirectory() / "hdd-icon.ico";
        g_app.nid.hIcon = (HICON)LoadImage(NULL, iconPath.string().c_str(),
                                           IMAGE_ICON, cx, cy, LR_LOADFROMFILE | LR_DEFAULTCOLOR);
    }
    if (!g_app.nid.hIcon) {
        g_app.nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    strcpy_s(g_app.nid.szTip, sizeof(g_app.nid.szTip), "HDD Toggle - Checking status...");

    const int MAX_RETRIES = 10;
    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        if (Shell_NotifyIcon(NIM_ADD, &g_app.nid)) return TRUE;
        Sleep(1000);
    }
    return FALSE;
}

void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &g_app.nid);
    if (g_app.nid.hIcon) {
        DestroyIcon(g_app.nid.hIcon);
        g_app.nid.hIcon = NULL;
    }
}

void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    if (g_app.hMenu) DestroyMenu(g_app.hMenu);
    g_app.hMenu = CreatePopupMenu();

    const char* statusText = DriveStateToStatusString(g_app.driveState);
    AppendMenu(g_app.hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, IDM_STATUS_DISPLAY, statusText);
    AppendMenu(g_app.hMenu, MF_SEPARATOR, 0, NULL);

    if (g_app.driveState == DriveState::Online) {
        AppendMenu(g_app.hMenu, MF_STRING, IDM_SLEEP_DRIVE, "Sleep Drive");
    } else {
        AppendMenu(g_app.hMenu, MF_STRING, IDM_WAKE_DRIVE, "Wake Drive");
    }

    AppendMenu(g_app.hMenu, MF_STRING, IDM_REFRESH_STATUS, "Refresh Status");
    AppendMenu(g_app.hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(g_app.hMenu, MF_STRING, IDM_EXIT, "Exit");

    if (g_app.isTransitioning) {
        EnableMenuItem(g_app.hMenu, (g_app.driveState == DriveState::Online) ? IDM_SLEEP_DRIVE : IDM_WAKE_DRIVE, MF_GRAYED);
    }

    SetForegroundWindow(hwnd);
    TrackPopupMenu(g_app.hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    g_app.lastMenuCloseTime = GetTickCount64();
}

void UpdateTrayIcon() {
    std::string tooltip = GetTooltipText(g_app.driveState);
    strcpy_s(g_app.nid.szTip, sizeof(g_app.nid.szTip), tooltip.c_str());

    HICON newIcon = LoadIconForDriveState(g_app.driveState);
    if (newIcon) {
        if (g_app.nid.hIcon) DestroyIcon(g_app.nid.hIcon);
        g_app.nid.hIcon = newIcon;
    }

    g_app.nid.uFlags = NIF_TIP | NIF_ICON;
    Shell_NotifyIcon(NIM_MODIFY, &g_app.nid);
}

HICON LoadIconForDriveState(DriveState state) {
    int iconResource;
    switch (state) {
        case DriveState::Online: iconResource = IDI_DRIVE_ON_ICON; break;
        case DriveState::Offline: iconResource = IDI_DRIVE_OFF_ICON; break;
        default: iconResource = IDI_MAIN_ICON; break;
    }

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    return (HICON)LoadImage(g_app.hInstance, MAKEINTRESOURCE(iconResource), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
}

void LoadConfiguration() {
    fs::path iniPath = GetExeDirectory() / "hdd-control.ini";
    std::string iniPathStr = iniPath.string();
    const char* iniPathC = iniPathStr.c_str();

    strcpy_s(g_app.config.targetSerial, sizeof(g_app.config.targetSerial), DEFAULT_TARGET_SERIAL);
    strcpy_s(g_app.config.targetModel, sizeof(g_app.config.targetModel), DEFAULT_TARGET_MODEL);
    g_app.config.periodicCheckMinutes = 10;
    g_app.config.postOperationCheckSeconds = 3;
    g_app.config.showNotifications = TRUE;
    g_app.config.debugMode = FALSE;

    if (fs::exists(iniPath)) {
        GetPrivateProfileString("Drive", "SerialNumber", g_app.config.targetSerial,
                               g_app.config.targetSerial, sizeof(g_app.config.targetSerial), iniPathC);
        GetPrivateProfileString("Drive", "Model", g_app.config.targetModel,
                               g_app.config.targetModel, sizeof(g_app.config.targetModel), iniPathC);
        DWORD checkMinutes = GetPrivateProfileInt("Timing", "PeriodicCheckMinutes", g_app.config.periodicCheckMinutes, iniPathC);
        if (checkMinutes >= 1) g_app.config.periodicCheckMinutes = checkMinutes;
        g_app.config.postOperationCheckSeconds = GetPrivateProfileInt("Timing", "PostOperationCheckSeconds",
                                                                 g_app.config.postOperationCheckSeconds, iniPathC);
        g_app.config.showNotifications = GetPrivateProfileInt("UI", "ShowNotifications", g_app.config.showNotifications, iniPathC);
        g_app.config.debugMode = GetPrivateProfileInt("Advanced", "DebugMode", g_app.config.debugMode, iniPathC);
    }
}

void AsyncPeriodicCheck(HWND hwnd) {
    ULONGLONG now = GetTickCount64();
    if (g_app.isTransitioning || (now - g_app.lastPeriodicCheck < 60000)) return;

    g_app.lastPeriodicCheck = now;
    DriveState newState = DetectDriveState();
    if (newState != g_app.driveState) {
        PostMessage(hwnd, WM_COMMAND, IDM_REFRESH_STATUS, 0);
    }
}

DriveState DetectDriveState() {
    ComInitializer comInit;
    if (!comInit.IsInitialized()) return DriveState::Unknown;

    CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_NONE,
                        RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

    ComPtr<IWbemLocator> pLoc;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                                IID_IWbemLocator, (LPVOID*)&pLoc))) return DriveState::Unknown;

    ComPtr<IWbemServices> pSvc;
    if (FAILED(pLoc->ConnectServer(_bstr_t(L"ROOT\\Microsoft\\Windows\\Storage"),
                                   NULL, NULL, 0, NULL, 0, 0, &pSvc))) return DriveState::Unknown;

    if (FAILED(CoSetProxyBlanket(pSvc.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                                 RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                                 NULL, EOAC_NONE))) return DriveState::Unknown;

    ComPtr<IEnumWbemClassObject> pEnumerator;
    if (FAILED(pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM MSFT_Disk"),
                               WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                               NULL, &pEnumerator))) return DriveState::Unknown;

    DriveState state = DriveState::Unknown;
    bool foundTarget = false;

    while (pEnumerator) {
        ComPtr<IWbemClassObject> pclsObj;
        ULONG uReturn = 0;
        if (pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn) != S_OK || uReturn == 0) break;

        VARIANT vtProp;
        VariantInit(&vtProp);

        if (SUCCEEDED(pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0)) && vtProp.vt == VT_BSTR) {
            _bstr_t serialNumber(vtProp.bstrVal, true);
            char serialStr[256];
            wcstombs_s(NULL, serialStr, 256, serialNumber, _TRUNCATE);
            std::string serial = TrimWhitespace(std::string(serialStr));

            if (SerialMatches(serial, g_app.config.targetSerial)) {
                foundTarget = true;
                VariantClear(&vtProp);

                if (SUCCEEDED(pclsObj->Get(L"IsOffline", 0, &vtProp, 0, 0)) && vtProp.vt == VT_BOOL) {
                    state = (vtProp.boolVal == VARIANT_TRUE) ? DriveState::Offline : DriveState::Online;
                }
                VariantClear(&vtProp);
                break;
            }
        }
        VariantClear(&vtProp);
    }

    return foundTarget ? state : DriveState::Offline;
}

void ShowBalloonTip(const char* title, const char* text, DWORD icon) {
    using namespace winrt::Windows::UI::Notifications;
    using namespace winrt::Windows::Data::Xml::Dom;

    try {
        static bool winrtInitialized = false;
        if (!winrtInitialized) {
            winrt::init_apartment();
            winrtInitialized = true;
        }

        wchar_t wtext[256];
        MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 256);

        std::wstring xml = L"<toast duration=\"short\"><visual><binding template=\"ToastGeneric\">";
        xml += L"<text>" + std::wstring(wtext) + L"</text>";
        xml += L"</binding></visual></toast>";

        XmlDocument doc;
        doc.LoadXml(xml);

        ToastNotification toast(doc);
        toast.ExpiresOnReboot(true);

        auto notifier = ToastNotificationManager::CreateToastNotifier(APP_AUMID);
        notifier.Show(toast);
    } catch (...) {
        g_app.nid.uFlags = NIF_INFO;
        g_app.nid.dwInfoFlags = icon;
        strcpy_s(g_app.nid.szInfoTitle, sizeof(g_app.nid.szInfoTitle), "");
        strcpy_s(g_app.nid.szInfo, sizeof(g_app.nid.szInfo), text);
        g_app.nid.uTimeout = 3000;
        Shell_NotifyIcon(NIM_MODIFY, &g_app.nid);
        g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    }
}

void StartProgressAnimation() {
    g_app.animationFrame = 0;
    g_app.animationTimer = SetTimer(g_app.hWnd, IDT_ANIMATION_TIMER, 500, NULL);
}

void StopProgressAnimation() {
    if (g_app.animationTimer) {
        KillTimer(g_app.hWnd, IDT_ANIMATION_TIMER);
        g_app.animationTimer = 0;
    }
    UpdateTrayIcon();
}

void AsyncDriveOperation(HWND hwnd, bool isWake) {
    // Run the internal command directly instead of spawning a process
    int result;
    if (isWake) {
        result = RunWake(0, nullptr);
    } else {
        result = RunSleep(0, nullptr);
    }

    PostMessage(hwnd, WM_COMMAND, isWake ? IDM_WAKE_COMPLETE : IDM_SLEEP_COMPLETE, (LPARAM)result);
}

void OnWakeDrive() {
    if (g_app.isTransitioning) return;

    g_app.isTransitioning = true;
    g_app.driveState = DriveState::Transitioning;
    UpdateTrayIcon();
    ShowBalloonTip("", "Waking drive...", NIIF_INFO);
    StartProgressAnimation();

    std::thread(AsyncDriveOperation, g_app.hWnd, true).detach();
}

void OnSleepDrive() {
    if (g_app.isTransitioning) return;

    g_app.isTransitioning = true;
    g_app.driveState = DriveState::Transitioning;
    UpdateTrayIcon();
    ShowBalloonTip("", "Sleeping drive...", NIIF_INFO);
    StartProgressAnimation();

    std::thread(AsyncDriveOperation, g_app.hWnd, false).detach();
}

void OnRefreshStatus() {
    g_app.driveState = DetectDriveState();
    UpdateTrayIcon();

    const char* statusMessage = DriveStateToString(g_app.driveState);
    ShowBalloonTip("", statusMessage, NIIF_INFO);
}

BOOL EnsureStartMenuShortcut() {
    wchar_t startMenuPath[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, startMenuPath))) return FALSE;

    std::wstring shortcutPath = std::wstring(startMenuPath) + L"\\HDD Toggle.lnk";
    if (GetFileAttributesW(shortcutPath.c_str()) != INVALID_FILE_ATTRIBUTES) return TRUE;

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return FALSE;

    BOOL success = FALSE;
    IShellLinkW* pShellLink = nullptr;
    IPersistFile* pPersistFile = nullptr;
    IPropertyStore* pPropertyStore = nullptr;

    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&pShellLink);
    if (FAILED(hr)) goto cleanup;

    pShellLink->SetPath(exePath);
    pShellLink->SetWorkingDirectory(GetExeDirectory().wstring().c_str());
    pShellLink->SetDescription(L"HDD Toggle - System tray app for hard drive power management");

    hr = pShellLink->QueryInterface(IID_IPropertyStore, (void**)&pPropertyStore);
    if (FAILED(hr)) goto cleanup;

    {
        PROPVARIANT propVar;
        hr = InitPropVariantFromString(APP_AUMID, &propVar);
        if (SUCCEEDED(hr)) {
            hr = pPropertyStore->SetValue(PKEY_AppUserModel_ID, propVar);
            PropVariantClear(&propVar);
        }
        if (FAILED(hr)) goto cleanup;
        hr = pPropertyStore->Commit();
        if (FAILED(hr)) goto cleanup;
    }

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

} // anonymous namespace

int LaunchTrayApp(HINSTANCE hInstance) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    InitDarkMode();
    SetCurrentProcessExplicitAppUserModelID(APP_AUMID);
    EnsureStartMenuShortcut();

    HANDLE hMutex = CreateMutex(NULL, TRUE, "Global\\HDDToggle_SingleInstance");
    if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);

        HWND existingWindow = FindWindow("HDDToggleTray", "HDD Toggle");
        if (existingWindow) {
            PostMessage(existingWindow, WM_COMMAND, IDM_REFRESH_STATUS, 0);
            try {
                using namespace winrt::Windows::UI::Notifications;
                using namespace winrt::Windows::Data::Xml::Dom;
                std::wstring xml = L"<toast duration=\"short\"><visual><binding template=\"ToastGeneric\">";
                xml += L"<text>Application is already running</text>";
                xml += L"</binding></visual></toast>";
                XmlDocument doc;
                doc.LoadXml(xml);
                ToastNotification toast(doc);
                toast.ExpiresOnReboot(true);
                auto notifier = ToastNotificationManager::CreateToastNotifier(APP_AUMID);
                notifier.Show(toast);
            } catch (...) {}
        }
        return 0;
    }

    g_app.hInstance = hInstance;
    g_app.wmTaskbarCreated = RegisterWindowMessage("TaskbarCreated");

    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "HDDToggleTray";

    wc.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON), IMAGE_ICON,
                                GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

    if (!wc.hIcon) wc.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(1), IMAGE_ICON,
                                               GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    if (!wc.hIconSm) wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(1), IMAGE_ICON,
                                                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    if (!wc.hIconSm) wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "Failed to register window class", "Error", MB_ICONERROR);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;
    }

    g_app.hWnd = CreateWindow("HDDToggleTray", "HDD Toggle", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    if (!g_app.hWnd) {
        MessageBox(NULL, "Failed to create window", "Error", MB_ICONERROR);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;
    }

    ApplyDarkModeToWindow(g_app.hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return (int)msg.wParam;
}

} // namespace commands
} // namespace hdd
