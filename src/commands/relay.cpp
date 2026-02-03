// USB Relay Controller for HDD Toggle
// Controls DCT Tech dual-channel USB HID relay
// Converted from relay.c to C++ for unified binary

#include "commands.h"
#include "hdd-toggle.h"
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

namespace hdd {
namespace commands {

namespace {

// Enumerate HID devices and open the first one that matches VENDOR_ID/PRODUCT_ID.
// Returns INVALID_HANDLE_VALUE on failure. On success, caller must CloseHandle().
HANDLE FindRelayDevice() {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO deviceInfo = SetupDiGetClassDevs(&hidGuid, NULL, NULL,
                                               DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (deviceInfo == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    SP_DEVICE_INTERFACE_DATA interfaceData = {sizeof(SP_DEVICE_INTERFACE_DATA)};

    // Stack allocation to avoid malloc/free
    BYTE buffer[1024];
    PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)buffer;

    // Enumerate all present HID device interfaces
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfo, NULL, &hidGuid, i, &interfaceData); i++) {
        DWORD requiredSize;
        // First call asks for the required buffer size
        SetupDiGetDeviceInterfaceDetail(deviceInfo, &interfaceData, NULL, 0, &requiredSize, NULL);

        if (requiredSize > sizeof(buffer)) continue;

        // cbSize must be set before retrieving interface details
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (SetupDiGetDeviceInterfaceDetail(deviceInfo, &interfaceData,
                                           detailData, requiredSize, NULL, NULL)) {

            // Open the HID device path for read/write, allowing shared access
            HANDLE device = CreateFile(detailData->DevicePath,
                                     GENERIC_READ | GENERIC_WRITE,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     NULL, OPEN_EXISTING, 0, NULL);

            if (device != INVALID_HANDLE_VALUE) {
                HIDD_ATTRIBUTES attributes = {sizeof(HIDD_ATTRIBUTES)};

                if (HidD_GetAttributes(device, &attributes)) {
                    if (attributes.VendorID == RELAY_VENDOR_ID &&
                        attributes.ProductID == RELAY_PRODUCT_ID) {
                        SetupDiDestroyDeviceInfoList(deviceInfo);
                        return device;
                    }
                }

                CloseHandle(device);
            }
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfo);
    return INVALID_HANDLE_VALUE;
}

// Control the relay with given parameters
// relayNum: 0 = all relays, 1 or 2 = specific relay
// stateOn: true = ON, false = OFF
bool ControlRelay(int relayNum, bool stateOn) {
    HANDLE device = FindRelayDevice();

    if (device == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error: USB relay not found\n");
        return false;
    }

    unsigned char report[RELAY_REPORT_SIZE] = {0};

    // Command lookup table to map (target, state) -> command byte.
    // Row 0 = all relays, Row 1 = single relay
    // Column 0 = OFF, Column 1 = ON
    // Values are specific to the relay firmware:
    //   0xFC=ALL OFF, 0xFE=ALL ON, 0xFD=SINGLE OFF, 0xFF=SINGLE ON
    static const unsigned char commands[2][2] = {{0xFC, 0xFE}, {0xFD, 0xFF}};

    report[0] = 0;  // Report ID
    report[1] = commands[relayNum > 0 ? 1 : 0][stateOn ? 1 : 0];
    if (relayNum > 0) report[2] = static_cast<unsigned char>(relayNum);

    BOOLEAN result = HidD_SetFeature(device, report, RELAY_REPORT_SIZE);
    CloseHandle(device);

    if (result) {
        printf("Relay %s: %s\n",
               relayNum == 0 ? "ALL" : (relayNum == 1 ? "1" : "2"),
               stateOn ? "ON" : "OFF");
        return true;
    } else {
        fprintf(stderr, "Error: Failed to send command\n");
        return false;
    }
}

void ShowRelayUsage() {
    printf("USB Relay Control (DCT Tech dual-channel relay)\n");
    printf("Usage: hdd-toggle relay <on|off>        (controls all relays)\n");
    printf("       hdd-toggle relay <1|2> <on|off>  (controls specific relay)\n");
}

} // anonymous namespace

// Public helper for internal use by wake/sleep commands
bool ControlRelayPower(bool on) {
    return ControlRelay(0, on);
}

int RunRelay(int argc, char* argv[]) {
    // argv[0] is "relay", actual args start at argv[1]
    // Adjust for the fact that we receive args after "relay" command

    if (argc < 1) {
        ShowRelayUsage();
        return EXIT_SUCCESS;  // Not an error - user likely wants to see usage
    }

    // Check for help flag
    if (argc == 1 && (strcmp(argv[0], "-h") == 0 ||
                      strcmp(argv[0], "--help") == 0 ||
                      strcmp(argv[0], "/?") == 0)) {
        ShowRelayUsage();
        return EXIT_SUCCESS;
    }

    // Support shorthand: "on" or "off" means control ALL relays
    if (argc == 1 && (_stricmp(argv[0], "on") == 0 || _stricmp(argv[0], "off") == 0)) {
        bool stateOn = (_stricmp(argv[0], "on") == 0);
        return ControlRelay(0, stateOn) ? EXIT_SUCCESS : EXIT_OPERATION_FAILED;
    }

    // Two arguments: relay number and state
    if (argc != 2) {
        fprintf(stderr, "Error: Invalid arguments\n");
        ShowRelayUsage();
        return EXIT_INVALID_ARGS;
    }

    // Parse relay number
    int relayNum = -1;
    if (_stricmp(argv[0], "all") == 0) relayNum = 0;
    else if (argv[0][0] == '1' && argv[0][1] == '\0') relayNum = 1;
    else if (argv[0][0] == '2' && argv[0][1] == '\0') relayNum = 2;
    else {
        fprintf(stderr, "Error: Invalid relay '%s' (use 1, 2, or all)\n", argv[0]);
        return EXIT_INVALID_ARGS;
    }

    // Parse state
    bool stateOn;
    if (_stricmp(argv[1], "on") == 0) stateOn = true;
    else if (_stricmp(argv[1], "off") == 0) stateOn = false;
    else {
        fprintf(stderr, "Error: Invalid state '%s' (use on or off)\n", argv[1]);
        return EXIT_INVALID_ARGS;
    }

    return ControlRelay(relayNum, stateOn) ? EXIT_SUCCESS : EXIT_OPERATION_FAILED;
}

} // namespace commands
} // namespace hdd
