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

## Current Status: ⚡ **MODERNIZATION IN PROGRESS**

### ✅ Working Features
1. **Silent drive detection** - WMI-based, no popup windows
2. **Accurate status tracking** - Online/Offline/Missing states  
3. **System tray integration** - Custom icon and context menus
4. **Toast notifications** - Operation feedback with system icons
5. **Hardware control** - Reliable relay operation
6. **Single instance** - Prevents multiple apps running
7. **Icon cache management** - Tools for clearing Windows icon caches
8. **Comprehensive ICO file** - All Windows standard sizes (16-256px)

### 🎯 **Recent Achievements (Session 2025-01-25)**

#### **Icon System Overhaul**
1. **✅ Resolved Windows Icon Caching Issues**:
   - **Root Cause**: Windows caches application icons by executable path
   - **Solution**: Created `hdd-control-gui-v2.exe` with new name to bypass cache
   - **Tools Added**: `clear-app-icon-cache.bat` for comprehensive cache clearing
   - **Result**: Toast notifications now show custom HDD icon in title bar

2. **✅ Multi-Size ICO File Creation**:
   - **Enhanced**: `convert-png-to-ico.bat` with ImageMagick integration
   - **Sizes Added**: 16, 20, 24, 32, 40, 48, 64, 128, 256 pixels (all 32-bit RGBA)
   - **File Size**: 143KB ICO with optimal Windows integration
   - **Benefit**: Crisp icons at all DPI levels, proper Windows Settings display

3. **✅ Modern Windows API Integration** (Partially Implemented):
   - **NOTIFYICON_VERSION_4**: Modern notification API support
   - **GUID Identity**: Stable icon identity for persistent Windows settings  
   - **WNDCLASSEX**: Enhanced window class with small icon support
   - **TaskbarCreated**: Explorer restart resilience
   - **Status**: Basic infrastructure added, requires stability tuning

#### **Development Process Insights**
4. **🔍 Windows API Complexity Discovered**:
   - **NIIF_USER Challenges**: Custom balloon icons often fail silently
   - **GUID Compatibility**: NIF_GUID can interfere with basic functionality
   - **Version Conflicts**: NOTIFYICON_VERSION_4 requires careful implementation
   - **Learning**: Modern features need gradual, tested integration

5. **🛠️ Build System Improvements**:
   - **Cache-Busting**: Executable versioning for icon cache invalidation
   - **Resource Management**: Proper `IDI_MAIN_ICON` constant definitions
   - **Compilation Pipeline**: Enhanced with modern resource handling

### 🔧 **Current Focus Areas**

#### **Stability Priority (In Progress)**
1. **Basic Functionality Restoration**:
   - **Issue**: Modern API changes broke tooltips and toast notifications
   - **Approach**: Simplified implementation to restore core features
   - **Status**: Debugging GUID/version compatibility issues
   - **Next**: Gradual re-introduction of modern features

2. **Tooltip System Debug**:
   - **Problem**: Hover tooltips not displaying after modernization
   - **Investigation**: NIF_GUID flag compatibility with NIF_TIP
   - **Solution**: Currently testing simplified flag combinations

#### **Future Enhancement Goals** 
3. **Dynamic Tray Icon States**:
   - Implement 'on' and 'off' icon variants to reflect drive state
   - Switch between `hdd-icon-on.ico` and `hdd-icon-off.ico` based on detection
   - Provide visual indication of drive status at a glance

4. **Elevated Privileges & UAC**:
   - Investigate running app with elevated permissions from startup
   - Eliminate UAC prompts for drive operations
   - Research Windows service approach or startup elevation

5. **Modern Context Menu**:
   - Current menu is blurry on high-DPI displays (resolution scaling issues)
   - Explore modern Windows APIs for context menus
   - Consider Windows 11-style menus with proper DPI awareness and theming
   - Reference: Built-in "Safely Remove Hardware" menu styling

6. **Code Signing**: Sign executables to eliminate Windows Defender delays

### 📚 **Technical Knowledge Gained**

#### **Windows Icon System Deep Dive**
- **Cache Behavior**: Windows aggressively caches icons by executable path
- **Multi-Context Icons**: Different Windows surfaces require different icon sizes
- **API Evolution**: Modern Windows APIs (v4) provide better integration but require careful implementation
- **Resource Loading**: `LoadImage()` with system metrics prevents scaling issues
- **GUID Identity**: Enables persistent user preferences but can conflict with basic functionality

#### **Development Best Practices Learned**
- **Incremental Modernization**: Add modern features gradually while maintaining working baseline
- **Cache Management**: Icon changes require comprehensive cache clearing and executable renaming
- **API Compatibility**: Modern Windows APIs need fallback chains for stability
- **Resource Sizing**: Always load icons with appropriate system metrics for context

## Development Environment
- **Location**: `C:\Users\jdlien\code\hdd-toggle`
- **Language**: C++ (migrated from C for WMI support)
- **Compiler**: Microsoft Visual Studio 2022 C++ compiler
- **Target OS**: Windows 10/11 with high-DPI support
- **Build**: `compile-gui.bat` for complete compilation
- **Current Version**: `hdd-control-gui-v2.exe` (589KB with enhanced ICO)

## Architecture Notes
- **No PowerShell Dependencies**: All detection uses native Windows APIs
- **COM Integration**: Proper COM initialization and cleanup for WMI
- **Resource Management**: Enhanced with `IDI_MAIN_ICON` constants and multi-size support
- **DPI Awareness**: Improved with proper icon sizing and system metrics
- **Cache Resilience**: Tools and strategies for Windows icon cache management
- **Modern API Support**: Infrastructure for NOTIFYICON_VERSION_4 (requires stability work)

---
*Status: Modernization In Progress - Core functionality maintained with enhanced icon system*  
*Last Updated: 2025-01-25*