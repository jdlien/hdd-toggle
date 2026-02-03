// Disk utilities for HDD Toggle
// Shared functions for drive detection and status

#include "core/disk.h"
#include "core/com.h"
#include "core/process.h"
#include "hdd-toggle.h"
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include <cstdio>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace hdd {
namespace core {

DriveInfo DetectDriveInfo(const std::string& targetSerial) {
    DriveInfo info;

    ComInitializer comInit;
    if (!comInit.IsInitialized()) {
        return info;
    }

    // Set COM security levels (ignore failure if already set)
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
            char serialStr[256];
            wcstombs_s(NULL, serialStr, 256, vtProp.bstrVal, _TRUNCATE);
            std::string serial = TrimWhitespace(std::string(serialStr));

            if (SerialMatches(serial, targetSerial)) {
                info.found = true;
                info.serialNumber = serial;
                VariantClear(&vtProp);

                // Get FriendlyName (model)
                hr = pclsObj->Get(L"FriendlyName", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                    char modelStr[256];
                    wcstombs_s(NULL, modelStr, 256, vtProp.bstrVal, _TRUNCATE);
                    info.model = TrimWhitespace(std::string(modelStr));
                }
                VariantClear(&vtProp);

                // Get Number
                hr = pclsObj->Get(L"Number", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr)) {
                    if (vtProp.vt == VT_I4) {
                        info.diskNumber = vtProp.lVal;
                    } else if (vtProp.vt == VT_UI4) {
                        info.diskNumber = static_cast<int>(vtProp.ulVal);
                    }
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

bool IsDiskOnline(const std::string& targetSerial, const std::string& targetModel) {
    char command[1024];
    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"$disk = Get-Disk | Where-Object { $_.SerialNumber -match '%s' -or $_.FriendlyName -match '%s' } -ErrorAction SilentlyContinue; "
        "if ($disk -and -not $disk.IsOffline) { exit 0 } else { exit 1 }\"",
        targetSerial.c_str(), targetModel.c_str());

    return ExecuteCommand(command, true) == 0;
}

} // namespace core
} // namespace hdd
