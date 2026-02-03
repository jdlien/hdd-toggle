# HDD Toggle - Claude Code Instructions

A Windows system tray application for controlling mechanical hard drive power via USB relay.

## Project Overview

This project provides safe, automated power control for a mechanical hard drive through:
1. **Physical relay control** - USB HID relay (DCT Tech 2-Channel) switches 12V and 5V power
2. **Windows integration** - WMI-based drive detection, safe ejection, and device management
3. **System tray GUI** - Modern Windows 11 tray app with toast notifications

### How It Works

1. **Wake sequence**: `hdd-control.exe` calls `wake-hdd.exe` which:
   - Activates USB relay to power on the HDD
   - Triggers Windows PnP device detection
   - Brings the disk online if needed

2. **Sleep sequence**: `hdd-control.exe` calls `sleep-hdd.exe` which:
   - Uses RemoveDrive.exe to safely eject the drive
   - Deactivates the relay to cut power
   - Confirms Windows recognizes the disconnection

3. **Status monitoring**: The tray app periodically checks drive presence via WMI queries to `MSFT_Disk`

## Project Structure

```
hdd-toggle/
├── src/                        # C/C++ source code
│   ├── hdd-control-gui.cpp     # Main tray application (C++17)
│   ├── relay.c                 # USB HID relay controller
│   ├── wake-hdd.c              # Drive wake utility
│   └── sleep-hdd.c             # Drive sleep utility
├── include/                    # Headers
│   └── hdd-utils.h             # Shared utilities and types
├── res/                        # Windows resources
│   ├── hdd-icon.rc             # Resource definitions (icons, manifest, version)
│   └── hdd-control.manifest    # Windows manifest for DPI/theme
├── assets/
│   ├── icons/                  # .ico files for tray and states
│   ├── images/                 # .png files (toast images, etc.)
│   └── source/                 # Source artwork (.afphoto)
├── scripts/
│   ├── build/                  # Compilation scripts
│   │   ├── compile-gui.bat     # Build the tray app
│   │   ├── compile.bat         # Build relay/wake/sleep utilities
│   │   └── compile-tests.bat   # Build and run unit tests
│   ├── hdd/                    # HDD control scripts
│   │   ├── wake-hdd.ps1        # PowerShell wake script
│   │   └── sleep-hdd.ps1       # PowerShell sleep script
│   └── util/                   # Utility scripts
│       ├── setup-startup.bat   # Add to Windows startup
│       ├── remove-startup.bat  # Remove from Windows startup
│       ├── check-startup.bat   # Check startup status
│       └── (icon conversion scripts)
├── bin/                        # Build output (gitignored)
│   ├── hdd-control.exe         # Main tray application
│   ├── relay.exe               # Relay controller
│   ├── wake-hdd.exe            # Wake utility
│   └── sleep-hdd.exe           # Sleep utility
├── tests/                      # Unit tests
│   ├── catch.hpp               # Catch2 test framework
│   ├── test_main.cpp           # Test runner entry point
│   └── test_utils.cpp          # Utility function tests
├── docs/                       # Documentation
├── hdd-control.ini             # Runtime configuration
├── README.md                   # User documentation
└── CHANGELOG.md                # Version history
```

## Build System

All build scripts work from the project root or from `scripts/build/`. They auto-detect their location and use `pushd`/`popd` to work from project root.

### Building

```batch
# Build the main GUI application (the one you usually want)
scripts\build\compile-gui.bat

# Build relay/wake/sleep C utilities
scripts\build\compile.bat

# Build and run unit tests
scripts\build\compile-tests.bat

# Run tests with coverage (requires OpenCppCoverage)
scripts\build\coverage.bat
scripts\build\coverage.bat --open   # Auto-open HTML report
```

### Toolchain Requirements

- **Microsoft Visual Studio 2022 Community** with C++ Desktop development
- **Windows SDK 10.0.26100.0** (or update paths in scripts)
- **MSVC Toolset 14.44.35207** (or update `VCPATH` in scripts)

The build scripts set up the environment themselves - no vcvars needed.

### Build Outputs

All executables go to `bin/`:
- `bin/hdd-control.exe` - Main tray application
- `bin/relay.exe` - USB relay controller
- `bin/wake-hdd.exe` - Drive wake utility
- `bin/sleep-hdd.exe` - Drive sleep utility

## Configuration

`hdd-control.ini` in the project root (or next to the exe):

```ini
[Drive]
SerialNumber=2VH7TM9L
Model=WDC WD181KFGX-68AFPN0

[Commands]
WakeCommand=wake-hdd.exe
SleepCommand=sleep-hdd.exe

[Timing]
PeriodicCheckMinutes=10
PostOperationCheckSeconds=3
```

## Development Guidelines

1. **Keep it simple** - Minimize dependencies, prefer Win32 APIs over frameworks
2. **Modern C++** - Use C++17 features (std::filesystem, std::thread)
3. **Static linking** - Default builds use `/MT` for standalone executables
4. **No external deps** - Everything except RemoveDrive.exe is self-contained

## Key Technical Details

### USB Relay Protocol
- VID/PID: 0x16C0/0x05DF (DCT Tech 2-Channel)
- 9-byte HID feature reports
- Commands: 0xFE (all on), 0xFC (all off), 0xFF/0xFD (single channel)

### Windows Integration
- Uses WMI `ROOT\Microsoft\Windows\Storage` namespace
- Queries `MSFT_Disk` for drive detection
- WinRT toast notifications (requires Start Menu shortcut with AUMID)
- Undocumented uxtheme.dll APIs for dark mode menus

### Icon Resources
- IDI_MAIN_ICON (100) - Default/unknown state
- IDI_DRIVE_ON_ICON (101) - Drive online
- IDI_DRIVE_OFF_ICON (102) - Drive offline

## Releases

When creating GitHub releases, the zip file **must** be named `hdd-toggle-windows.zip` (no version number). This allows the README's download link to always point to the latest release:

```
https://github.com/jdlien/hdd-toggle/releases/latest/download/hdd-toggle-windows.zip
```

The zip should contain:
- `hdd-toggle.exe` - x64 build
- `hdd-toggle-arm64.exe` - ARM64 build
- `install.bat` / `uninstall.bat`
- `hdd-control.ini`
- `README.md` / `CHANGELOG.md`

## External Tools

- **ImageMagick 7**: `C:\Program Files\ImageMagick-7.1.2-Q16-HDRI` - for icon conversion
- **RemoveDrive.exe**: Must be on PATH - used by sleep-hdd for safe ejection
  Download from: https://www.uwe-sieber.de/drivetools_e.html

## Running .bat Files

Use PowerShell to run batch files:
```powershell
pwsh -Command "& scripts\build\compile-gui.bat"
```

Or from cmd:
```batch
scripts\build\compile-gui.bat
```
