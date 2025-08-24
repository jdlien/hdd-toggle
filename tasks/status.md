# HDD Toggle Project Status

## Project Overview
This project aims to control an 18TB WDC WD181KFGX-68AFPN0 hard drive (Serial: 2VH7TM9L) by:
1. Spinning down/sleeping the drive via software commands
2. Potentially using a USB relay to physically control power
3. Automating the process for power management

## Current State Analysis

### USB Relay Control Scripts (Python)
Multiple iterations of relay control scripts have been developed:

#### Core Files:
- **relay_control.py**: Main control script with command-line interface
  - Supports relay 1, 2, or all relays
  - Commands: on/off 
  - Test sequence functionality
  - Uses HID library with VID: 0x16C0, PID: 0x05DF (DCT Tech relay)

- **relay_final_test.py**: Comprehensive testing script
  - Tests multiple report IDs (0, 1, 2)
  - Tests various command formats (V-USB, binary mask, alternative formats)
  - Tests both feature reports and output reports
  - Designed to identify working command format through audible relay clicks

- **relay_working.py**: Object-oriented implementation
  - USBRelay class with connect/disconnect methods
  - Sends commands in multiple formats to ensure activation
  - Implements both feature and write report methods

#### Other Test Files:
- relay.py, relay_debug.py, relay_feature.py, test_relay.py, dcttech_relay.py
  - Various test implementations and debugging attempts

### PowerShell Scripts
- **sleep-hdd.ps1**: Sophisticated HDD power management
  - Multiple modes: OfflineThenSpinDown (default), Offline, Disable
  - Uses smartctl for ATA standby/sleep commands
  - Fallback to Windows PnP device management
  - Verification option to confirm standby without waking drive
  - Safe guards against system disk operations

- **wake-hdd.ps1**: Companion script to re-enable the drive

- **debug-disks.ps1**: Disk enumeration/debugging utility

### Legacy Scripts
- **sleep-hdd.exe**: Compiled executable version
- **sleep-hdd.vbs**: VBScript implementation

## Technical Findings

### USB Relay Status
- Device successfully connects (VID: 0x16C0, PID: 0x05DF)
- Multiple command formats tested but no confirmed working format yet
- Need to listen for relay clicks to identify correct protocol
- Common formats tried:
  - V-USB standard: 0xFE (on), 0xFC (off)
  - Relay-specific: 0xFF/0xFD with relay number
  - Binary mask: 0x00, 0x03 (both relays)
  - Alternative formats: 0xA0, 0x05, 0x51 prefixes

### HDD Control Status
- PowerShell script successfully spins down drive using smartctl
- Can offline and disable drive through Windows disk management
- Multiple safeguards prevent accidental system disk operations
- Verification system confirms standby without waking drive

## Critical Issue: BSOD Incident
A Blue Screen of Death occurred during development. Analysis:

### Unlikely Causes:
- **Python HID scripts**: Very unlikely to cause BSOD
  - HID library uses standard Windows HID API
  - User-space operations only
  - No kernel-level access
  - Worst case would be application crash, not system crash

### Possible Causes:
1. **USB driver issue**: Faulty or incompatible USB relay driver
2. **Disk operations**: Aggressive offline/disable operations on wrong disk
3. **Hardware problem**: USB relay hardware fault or power issue
4. **Coincidental system issue**: Unrelated Windows update, driver conflict, or hardware failure

## Next Steps

### Immediate Actions:
1. ✅ Check Windows Event Viewer for BSOD details (WhoCrashed, BlueScreenView)
2. ✅ Verify USB relay is properly connected and powered
3. ✅ Test relay control with minimal script (relay_control.py test mode)
4. ✅ Monitor for relay clicking sounds to confirm working command

### Completed Goals:
1. ✅ **Identify working relay command format**
   - Report ID 1 with 0xFE/0xFC (all) and 0xFF/0xFD (individual) confirmed working
   - Created minimal relay.py script with confirmed commands
   - Implemented UAC elevation for device detection

2. ✅ **Create unified control scripts**
   - wake-hdd.ps1: Powers up relays + device detection + drive online checks
   - sleep-hdd.ps1: Takes drive offline + powers down relays + verification
   - Both scripts work as regular user (with UAC prompts when needed)

3. ✅ **Performance optimization**
   - Simultaneous relay control (no staggering delays)
   - UAC elevation for reliable device detection
   - Minimal, focused scripts with clean output

### Future Improvements (TODO):
1. **Create self-contained exe for the relay.py control**
   - Use PyInstaller or similar to create standalone executable
   - Eliminate Python runtime dependency
   - Single-file deployment option

2. **Extract common functionality to library**
   - Create shared PowerShell module for device detection
   - Common relay control functions
   - Reusable components for other projects

3. **Create a nicer GUI for HDD control**
   - Simple Windows Forms or WPF application
   - One-click wake/sleep buttons
   - Status indicators for drive state
   - System tray integration option

## Safety Recommendations
1. Always verify target disk serial number before operations
2. Never run disk operations with administrative privileges unless necessary
3. Test relay commands without connected load first
4. Keep backups of working scripts before modifications
5. Use virtual environment for Python dependencies

## Current Working Directory
`C:\Users\jdlien\code\hdd-toggle`

---
*Last Updated: 2025-08-24*