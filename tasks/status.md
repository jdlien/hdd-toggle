# HDD Toggle Project Status

## Project Overview
This project provides elegant power control for a mechanical hard drive in Windows through USB relay control. The system safely powers down and powers up an 18TB WDC WD181KFGX-68AFPN0 hard drive (Serial: 2VH7TM9L) by:

1. **Physical power control** via USB relay (DCT Tech dual-channel relay)
2. **Software integration** with Windows device management
3. **Safe drive state management** with proper offline/online handling

## Current Implementation

### Core Files

#### **relay.c** + **relay.exe**
- **Purpose**: USB HID relay controller for DCT Tech dual-channel relay
- **Features**:
  - Enumerates HID devices by VID/PID (0x16C0/0x05DF)
  - Sends HID feature reports to control relay channels
  - Command-line interface: `relay {1|2|all} {on|off}`
  - Robust error handling and device verification

#### **compile.bat**
- **Purpose**: Compiles relay.c using Microsoft Visual Studio C compiler
- **Features**:
  - Static build (standalone executable) or dynamic build (smaller with dependencies)
  - Automatic cleanup of intermediate files
  - Build environment detection and setup

#### **wake-hdd.ps1**
- **Purpose**: Complete drive wake-up sequence
- **Features**:
  - Powers up relays using relay.exe
  - Triggers Windows device detection (with UAC elevation if needed)
  - Waits for drive to be detected and come online
  - Brings disk online if found offline
  - Status verification and user feedback

#### **sleep-hdd.ps1**
- **Purpose**: Safe drive shutdown sequence
- **Features**:
  - Optional disk offline before power down (use `-Offline` parameter)
  - Powers down relays using relay.exe
  - Verifies drive disconnection from Windows
  - Graceful handling of already-offline drives
  - Command-line parameters for different shutdown modes

## Technical Architecture

### USB Relay Control
- **Hardware**: DCT Tech dual-channel USB HID relay
- **Communication**: Windows HID API via feature reports
- **Protocol**: 9-byte reports with command lookup table
  - Report ID: 0
  - Commands: 0xFC (all off), 0xFE (all on), 0xFD (single off), 0xFF (single on)
  - Channel specification for single relay operations

### Windows Integration
- **Device Detection**: PnP device enumeration with fallback methods
- **Drive Management**: Windows Disk Management API integration
- **UAC Elevation**: Automatic privilege escalation when needed for device operations
- **Error Handling**: Comprehensive status checking and user feedback

### Safety Features
- **Drive Identification**: Serial number verification (2VH7TM9L)
- **State Verification**: Checks current drive status before operations
- **Graceful Degradation**: Continues operation even if some steps fail
- **User Feedback**: Clear status messages throughout all operations

## Project Status: ✅ **COMPLETE & FUNCTIONAL**

### ✅ Completed Features
1. **Reliable USB relay control** - C implementation with confirmed working protocol
2. **Complete wake sequence** - Power up, detect, and bring drive online
3. **Safe sleep sequence** - Optional offline, power down, verify disconnection
4. **Windows integration** - PnP device detection, disk management, UAC elevation
5. **Error handling** - Comprehensive status checking and user feedback
6. **Clean codebase** - Removed legacy/test files, proper version control

### Current Workflow
1. **To wake drive**: Run `.\wake-hdd.ps1`
   - Powers up both relays
   - Triggers device detection (may prompt for admin access)
   - Waits for Windows to detect and initialize drive
   - Reports when drive is ready for use

2. **To sleep drive**: Run `.\sleep-hdd.ps1 [-Offline]`
   - Optionally takes disk offline first (if `-Offline` specified)
   - Powers down both relays
   - Verifies drive has disconnected from Windows
   - Reports completion status

## Development Environment
- **Location**: `C:\Users\jdlien\code\hdd-toggle`
- **Compiler**: Microsoft Visual Studio C++ (via compile.bat)
- **Target OS**: Windows 10/11
- **Version Control**: Git with proper line ending handling (.gitattributes)

## Future Enhancements (Optional)
1. **GUI Application**: Simple Windows Forms app with wake/sleep buttons
2. **System Tray Integration**: Background service with notification area icon
3. **Scheduled Tasks**: Automatic sleep/wake based on time or usage patterns
4. **Multiple Drive Support**: Extend to handle additional drives with different relays

---
*Status: Production Ready*  
*Last Updated: 2025-08-24*