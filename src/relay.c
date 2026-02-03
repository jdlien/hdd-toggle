// USB Relay Controller for Windows (DCT Tech dual-channel relay)
// - Enumerates HID devices to find the relay by VID/PID
// - Sends HID feature reports to switch channels ON/OFF
// Build: use compile.bat; links hid.lib and setupapi.lib
#include <windows.h>  // Win32 base API (HANDLE, GUID, CreateFile)
#include <hidsdi.h>   // HID APIs (HidD_GetHidGuid, GetAttributes, SetFeature)
#include <setupapi.h> // Device enumeration (SetupDi* functions)
#include <stdio.h>    // printf/fprintf
#include <string.h>   // strcmp/_stricmp

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

// Device IDs for the specific USB HID relay we control.
// Change these if your hardware uses different Vendor/Product IDs.
#define VENDOR_ID 0x16C0
#define PRODUCT_ID 0x05DF
// HID feature report size expected by the device (byte 0 is Report ID)
#define REPORT_SIZE 9

// Enumerate HID devices and open the first one that matches VENDOR_ID/PRODUCT_ID.
// Returns INVALID_HANDLE_VALUE on failure. On success, caller must CloseHandle().
HANDLE find_relay_device() {
    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);

    HDEVINFO device_info = SetupDiGetClassDevs(&hid_guid, NULL, NULL,
                                               DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (device_info == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    SP_DEVICE_INTERFACE_DATA interface_data = {sizeof(SP_DEVICE_INTERFACE_DATA)};

    // Stack allocation to avoid malloc/free
    // If required_size is larger, we skip that interface (see below).
    BYTE buffer[1024];
    PSP_DEVICE_INTERFACE_DETAIL_DATA detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA)buffer;

    // Enumerate all present HID device interfaces
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(device_info, NULL, &hid_guid, i, &interface_data); i++) {
        DWORD required_size;
        // First call asks for the required buffer size (common SetupAPI pattern)
        SetupDiGetDeviceInterfaceDetail(device_info, &interface_data, NULL, 0, &required_size, NULL);

        if (required_size > sizeof(buffer)) continue;  // Skip if buffer too small

        // cbSize must be set before retrieving interface details
        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (SetupDiGetDeviceInterfaceDetail(device_info, &interface_data,
                                           detail_data, required_size, NULL, NULL)) {

            // Open the HID device path for read/write, allowing shared access
            HANDLE device = CreateFile(detail_data->DevicePath,
                                     GENERIC_READ | GENERIC_WRITE,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     NULL, OPEN_EXISTING, 0, NULL);

            if (device != INVALID_HANDLE_VALUE) {
                HIDD_ATTRIBUTES attributes = {sizeof(HIDD_ATTRIBUTES)};
                // Verify VID/PID to ensure we're talking to the intended device

                if (HidD_GetAttributes(device, &attributes)) {
                    if (attributes.VendorID == VENDOR_ID && attributes.ProductID == PRODUCT_ID) {
                        SetupDiDestroyDeviceInfoList(device_info);
                        return device;
                    }
                }

                CloseHandle(device);
            }
        }
    }

    SetupDiDestroyDeviceInfoList(device_info);

    return INVALID_HANDLE_VALUE;
}

int control_relay(int relay_num, int state_on) {
    HANDLE device = find_relay_device();

    if (device == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error: USB relay not found\n");
        return 0;
    }

    unsigned char report[REPORT_SIZE] = {0};

    // Command lookup table to map (target, state) -> command byte.
    // Row 0 = all relays, Row 1 = single relay
    // Column 0 = OFF, Column 1 = ON
    // Values are specific to the relay firmware:
    //   0xFC=ALL OFF, 0xFE=ALL ON, 0xFD=SINGLE OFF, 0xFF=SINGLE ON
    static const unsigned char commands[2][2] = {{0xFC, 0xFE}, {0xFD, 0xFF}};

    report[0] = 0;  // Report ID (often 0 for simple HID devices)
    report[1] = commands[relay_num > 0][state_on];  // boolean indexing
    if (relay_num > 0) report[2] = relay_num;  // channel number when targeting a single relay

    BOOLEAN result = HidD_SetFeature(device, report, REPORT_SIZE);
    CloseHandle(device);

    if (result) {
        printf("Relay %s: %s\n",
               relay_num == 0 ? "ALL" : (relay_num == 1 ? "1" : "2"),
               state_on ? "ON" : "OFF");
        return 1;
    } else {
        fprintf(stderr, "Error: Failed to send command\n");
        return 0;
    }
}

// Print usage help to stdout
void show_usage() {
    printf("USB Relay Control (DCT Tech dual-channel relay)\n");
    printf("Usage: relay <on|off> (controls all relays)\n");
    printf("   or: relay <1|2|all> <on|off> (controls individual relays)\n");
}

int main(int argc, char* argv[]) {
    // Show help if no args or help flag
    if (argc == 1) {
        show_usage();
        return 0;  // Not an error - user likely wants to see usage
    }

    if (argc == 2 && (strcmp(argv[1], "-h") == 0 ||
                      strcmp(argv[1], "--help") == 0 ||
                      strcmp(argv[1], "/?") == 0)) {
        show_usage();
        return 0;  // Explicit help request
    }

    // Parse arguments to pass to control_relay

    // Support shorthand: "on" or "off" means control ALL relays
    if (argc == 2 && (_stricmp(argv[1], "on") == 0 || _stricmp(argv[1], "off") == 0)) {
        // Shorthand detected - control all relays
        int state_on = (_stricmp(argv[1], "on") == 0) ? 1 : 0;
        return control_relay(0, state_on) ? 0 : 1;
    }

    // Check for correct number of arguments (must be 3 for normal usage)
    if (argc != 3) {
        fprintf(stderr, "Error: Invalid arguments\n");
        show_usage();
        return 1;  // This is an error - wrong usage
    }

    // Parse relay number using simple string comparisons
    int relay_num = -1;
    if (_stricmp(argv[1], "all") == 0) relay_num = 0;
    else if (argv[1][0] == '1' && argv[1][1] == '\0') relay_num = 1;
    else if (argv[1][0] == '2' && argv[1][1] == '\0') relay_num = 2;
    else fprintf(stderr, "Error: Invalid relay '%s' (use 1, 2, or all)\n", argv[1]);

    // Parse state
    int state_on = -1;
    if (_stricmp(argv[2], "on") == 0) state_on = 1;
    else if (_stricmp(argv[2], "off") == 0) state_on = 0;
    else {
        fprintf(stderr, "Error: Invalid state '%s' (use on or off)\n", argv[2]);
        return 1;
    }

    // Execute command
    return control_relay(relay_num, state_on) ? 0 : 1;
}