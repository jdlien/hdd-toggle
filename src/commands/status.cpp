// Status Command for HDD Toggle
// Shows current drive status, with optional JSON output for scripting
// New command for unified binary

#include "commands.h"
#include "hdd-toggle.h"
#include "hdd-utils.h"
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace hdd {
namespace commands {

namespace {

struct StatusOptions {
    bool help = false;
    bool json = false;
};

struct DriveInfo {
    DriveState state = DriveState::Unknown;
    std::string serialNumber;
    std::string model;
    int diskNumber = -1;
    bool found = false;
};

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

StatusOptions ParseStatusArgs(int argc, char* argv[]) {
    StatusOptions opts;

    for (int i = 0; i < argc; i++) {
        if (_stricmp(argv[i], "-help") == 0 || _stricmp(argv[i], "-h") == 0 ||
            _stricmp(argv[i], "/?") == 0 || _stricmp(argv[i], "--help") == 0) {
            opts.help = true;
        }
        else if (_stricmp(argv[i], "--json") == 0 || _stricmp(argv[i], "-j") == 0) {
            opts.json = true;
        }
    }

    return opts;
}

void ShowStatusUsage() {
    printf("Drive Status - Show current hard drive status\n\n");
    printf("Usage: hdd-toggle status [--json] [-h|--help]\n\n");
    printf("Options:\n");
    printf("  --json, -j   Output in JSON format for scripting\n");
    printf("  -h, --help   Show this help message\n\n");
    printf("Target: %s (Serial: %s)\n", DEFAULT_TARGET_MODEL, DEFAULT_TARGET_SERIAL);
}

// Detect drive state using WMI
DriveInfo DetectDriveInfo() {
    DriveInfo info;

    // RAII COM initialization
    ComInitializer comInit;
    if (!comInit.IsInitialized()) {
        return info;
    }

    // Set COM security levels
    CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_NONE,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL
    );

    ComPtr<IWbemLocator> pLoc;
    HRESULT hres = CoCreateInstance(
        CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) return info;

    ComPtr<IWbemServices> pSvc;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\Microsoft\\Windows\\Storage"),
        NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) return info;

    hres = CoSetProxyBlanket(
        pSvc.Get(),
        RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE);
    if (FAILED(hres)) return info;

    ComPtr<IEnumWbemClassObject> pEnumerator;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSFT_Disk"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);
    if (FAILED(hres)) return info;

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
            std::string serial = TrimWhitespace(std::string(serialStr));

            if (SerialMatches(serial, DEFAULT_TARGET_SERIAL)) {
                info.found = true;
                info.serialNumber = serial;
                VariantClear(&vtProp);

                // Get Model/FriendlyName
                hr = pclsObj->Get(L"FriendlyName", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                    char modelStr[256];
                    wcstombs_s(NULL, modelStr, 256, vtProp.bstrVal, _TRUNCATE);
                    info.model = TrimWhitespace(std::string(modelStr));
                }
                VariantClear(&vtProp);

                // Get Number
                hr = pclsObj->Get(L"Number", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr) && vtProp.vt == VT_I4) {
                    info.diskNumber = vtProp.lVal;
                } else if (SUCCEEDED(hr) && vtProp.vt == VT_UI4) {
                    info.diskNumber = static_cast<int>(vtProp.ulVal);
                }
                VariantClear(&vtProp);

                // Get IsOffline property
                hr = pclsObj->Get(L"IsOffline", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr) && vtProp.vt == VT_BOOL) {
                    info.state = (vtProp.boolVal == VARIANT_TRUE) ? DriveState::Offline : DriveState::Online;
                }
                VariantClear(&vtProp);
                break;
            }
        }
        VariantClear(&vtProp);
    }

    return info;
}

void OutputJson(const DriveInfo& info) {
    if (!info.found) {
        printf("{\"status\":\"offline\",\"found\":false}\n");
        return;
    }

    const char* status = (info.state == DriveState::Online) ? "online" : "offline";
    printf("{\"status\":\"%s\",\"found\":true,\"serial\":\"%s\",\"model\":\"%s\",\"disk\":%d}\n",
           status,
           info.serialNumber.c_str(),
           info.model.c_str(),
           info.diskNumber);
}

void OutputText(const DriveInfo& info) {
    if (!info.found) {
        printf("Drive: OFFLINE (not detected)\n");
        printf("Target: %s (Serial: %s)\n", DEFAULT_TARGET_MODEL, DEFAULT_TARGET_SERIAL);
        return;
    }

    const char* status = (info.state == DriveState::Online) ? "ONLINE" : "OFFLINE";
    printf("Drive: %s\n", status);
    printf("Model: %s\n", info.model.c_str());
    printf("Serial: %s\n", info.serialNumber.c_str());
    printf("Disk Number: %d\n", info.diskNumber);
}

} // anonymous namespace

int RunStatus(int argc, char* argv[]) {
    StatusOptions opts = ParseStatusArgs(argc, argv);

    if (opts.help) {
        ShowStatusUsage();
        return EXIT_SUCCESS;
    }

    DriveInfo info = DetectDriveInfo();

    if (opts.json) {
        OutputJson(info);
    } else {
        OutputText(info);
    }

    return EXIT_SUCCESS;
}

} // namespace commands
} // namespace hdd
