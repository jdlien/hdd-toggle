#define UNICODE
#define _UNICODE

#include <windows.h>
#include <cfgmgr32.h>
#include <stdio.h>

static void print_usage(void) {
	fwprintf(stderr, L"Usage: eject.exe <PNP_INSTANCE_ID>\n");
	fwprintf(stderr, L"Example: eject.exe PCI\\VEN_144D&DEV_A808&SUBSYS...\\...\n");
}

int wmain(int argc, wchar_t** argv) {
	if (argc < 2) {
		print_usage();
		return 2;
	}

	const wchar_t* instanceId = argv[1];
	DEVINST devInst = 0;
	CONFIGRET cr;

	cr = CM_Locate_DevNodeW(&devInst, (DEVINSTID_W)instanceId, 0);
	if (cr != CR_SUCCESS) {
		// Try locating phantom node in case device just transitioned
		cr = CM_Locate_DevNodeW(&devInst, (DEVINSTID_W)instanceId, CM_LOCATE_DEVNODE_PHANTOM);
		if (cr != CR_SUCCESS) {
			fwprintf(stderr, L"Failed to locate device node (CR=0x%X)\n", cr);
			return 1;
		}
	}

	// First try a direct eject request
	PNP_VETO_TYPE vetoType = PNP_VetoTypeUnknown;
	wchar_t vetoName[512];
	vetoName[0] = L'\0';

	cr = CM_Request_Device_EjectW(devInst, &vetoType, vetoName, (ULONG)(sizeof(vetoName) / sizeof(vetoName[0])), 0);
	if (cr == CR_SUCCESS) {
		wprintf(L"Eject requested successfully.\n");
		return 0;
	}

	// Fallback: query-and-remove subtree (sometimes more permissive)
	vetoType = PNP_VetoTypeUnknown;
	vetoName[0] = L'\0';
	cr = CM_Query_And_Remove_SubTreeW(devInst, &vetoType, vetoName, (ULONG)(sizeof(vetoName) / sizeof(vetoName[0])), 0);
	if (cr == CR_SUCCESS) {
		wprintf(L"Query-and-remove requested successfully.\n");
		return 0;
	}

	fwprintf(stderr, L"Safe removal vetoed or failed (CR=0x%X, VetoType=%d, VetoName=%s)\n", cr, (int)vetoType, vetoName);
	return 1;
}
