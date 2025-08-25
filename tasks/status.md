# HDD Toggle Project Status

## Project Overview
This project provides elegant power control for a mechanical hard drive in Windows through USB relay control. The system safely powers down and powers up an 18TB WDC WD181KFGX-68AFPN0 hard drive (Serial: 2VH7TM9L) via:

1. **Physical power control** via USB relay (DCT Tech dual-channel relay)
2. **Native Windows integration** with WMI-based drive detection
3. **System tray GUI** with toast notifications and context menus

## Current Implementation

### Core Components

#### **hdd-control-gui.exe** (Primary Interface)
- **Language**: C++ with Windows COM/WMI APIs
- **Features**:
  - System tray application with custom HDD icon
  - Native WMI-based drive detection (no PowerShell windows)
  - Context menu with Wake/Sleep/Refresh/Exit options
  - Toast notifications for operation feedback
  - Single instance protection
  - Immediate status detection on startup

#### **wake-hdd.exe** + **sleep-hdd.exe** (Command Line Tools)
- **Purpose**: Direct command-line control of drive power state
- **Features**: Fast native implementations with UAC elevation support

#### **relay.exe** (Hardware Control)
- **Purpose**: USB HID relay controller for DCT Tech dual-channel relay
- **Interface**: `relay {1|2|all} {on|off}`

### Technical Architecture

#### Drive Detection
- **Method**: Native WMI queries using COM (`MSFT_Disk` class)
- **Performance**: ~50ms detection time vs ~2000ms PowerShell
- **Capabilities**: 
  - Online/Offline state detection
  - Missing drive detection (powered off)
  - Silent operation (no popup windows)

#### Hardware Control  
- **Protocol**: USB HID feature reports to DCT Tech relay
- **Safety**: Drive identification by serial number (2VH7TM9L)
- **Integration**: Seamless with Windows device management

## Current Status: ✅ **FUNCTIONAL WITH PENDING IMPROVEMENTS**

### ✅ Working Features
1. **Silent drive detection** - WMI-based, no popup windows
2. **Accurate status tracking** - Online/Offline/Missing states
3. **System tray integration** - Custom icon and context menus
4. **Toast notifications** - Operation feedback with system icons
5. **Hardware control** - Reliable relay operation
6. **Single instance** - Prevents multiple apps running

### 🔧 **Pending Tasks & Improvements**

#### **High Priority UI Issues**
1. **Icon Display Problems**:
   - Toast notifications show generic system icons instead of custom HDD icon
   - Taskbar settings "Other system tray icons" shows generic icon (possibly cached)
   - Need to investigate custom icon implementation for notifications

2. **Dynamic Tray Icon States**:
   - Implement 'on' and 'off' icon variants to reflect drive state
   - Switch between `hdd-icon-on.ico` and `hdd-icon-off.ico` based on detection
   - Provide visual indication of drive status at a glance

3. **Tooltip Status Display**:
   - Currently shows "Status Unknown" instead of actual drive state
   - Should display "Drive Online", "Drive Offline", or "Drive Not Found"

#### **User Experience Improvements**
4. **Elevated Privileges & UAC**:
   - Investigate running app with elevated permissions from startup
   - Eliminate UAC prompts for drive operations
   - Research Windows service approach or startup elevation

5. **Modern Context Menu**:
   - Current menu is blurry on high-DPI displays (resolution scaling issues)
   - Explore modern Windows APIs for context menus
   - Consider Windows 11-style menus with proper DPI awareness and theming
   - Reference: Built-in "Safely Remove Hardware" menu styling

#### **Code Quality**
6. **Code Signing**: Sign executables to eliminate Windows Defender delays

## Development Environment
- **Location**: `C:\Users\jdlien\code\hdd-toggle`
- **Language**: C++ (migrated from C for WMI support)
- **Compiler**: Microsoft Visual Studio 2022 C++ compiler
- **Target OS**: Windows 10/11 with high-DPI support
- **Build**: `compile-gui.bat` for complete compilation

## Architecture Notes
- **No PowerShell Dependencies**: All detection uses native Windows APIs
- **COM Integration**: Proper COM initialization and cleanup for WMI
- **Resource Management**: Multiple icon resource IDs for compatibility
- **DPI Awareness**: Needs improvement for high-resolution displays

---
*Status: Functional with UX Improvements Needed*  
*Last Updated: 2025-01-02*