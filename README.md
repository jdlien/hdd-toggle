# HDD Toggle

A Windows system tray application for controlling mechanical hard drive power via USB relay.

![Windows 11](https://img.shields.io/badge/Windows-11-0078D6?logo=windows)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)
![Coverage](https://img.shields.io/badge/coverage-100%25-brightgreen)
![License](https://img.shields.io/badge/license-MIT-blue)

## Overview

HDD Toggle provides safe, automated power control for a mechanical hard drive through:
- **Physical relay switching** - USB HID relay cuts 12V and 5V power
- **Windows integration** - WMI-based drive detection and safe ejection
- **System tray GUI** - Modern Windows 11 app with dark mode and toast notifications

Perfect for NAS drives, backup drives, or any HDD you want to power down when not in use to reduce noise, heat, and wear.

## Features

- **One-click wake/sleep** from system tray
- **Automatic status detection** via WMI
- **Windows 11 dark mode** support
- **Toast notifications** for operation feedback
- **Configurable** via INI file
- **Runs at startup** (optional Task Scheduler integration)

## Quick Start

### Requirements

- Windows 10/11
- [DCT Tech 2-Channel USB HID Relay](https://www.amazon.ca/dp/B0DKBY5YM1) (or compatible)
- Hard drive wired through relay contacts
- [RemoveDrive.exe](https://www.uwe-sieber.de/drivetools_e.html) on PATH (for safe ejection)

### Installation

1. Download the latest release or build from source
2. Place executables in a folder (e.g., `C:\bin\`):
   - `hdd-control.exe` - Main tray application
   - `wake-hdd.exe` - Wake utility
   - `sleep-hdd.exe` - Sleep utility
   - `relay.exe` - Relay controller
3. Edit `hdd-control.ini` with your drive's serial number
4. Run `hdd-control.exe`

### Configuration

Create `hdd-control.ini` next to the executable:

```ini
[Drive]
# Find your drive's serial with: wmic diskdrive get serialnumber
SerialNumber=YOUR_SERIAL_HERE
Model=Your Drive Model

[Commands]
WakeCommand=wake-hdd.exe
SleepCommand=sleep-hdd.exe

[Timing]
PeriodicCheckMinutes=10
PostOperationCheckSeconds=3

[UI]
ShowNotifications=true
```

## Usage

### System Tray App

Right-click (or left-click) the tray icon to:
- **Wake Drive** - Power on and detect the drive
- **Sleep Drive** - Safely eject and power off
- **Refresh Status** - Re-check drive state
- **Exit** - Close the application

The icon changes to indicate drive status (on/off/transitioning).

### Command Line Tools

```powershell
# Wake the drive
.\wake-hdd.exe

# Sleep the drive
.\sleep-hdd.exe

# Manual relay control
.\relay.exe all on    # Power on
.\relay.exe all off   # Power off
.\relay.exe 1 on      # Channel 1 only
```

### PowerShell Scripts (Alternative)

```powershell
# Wake with device detection
.\scripts\hdd\wake-hdd.ps1

# Sleep with safe removal
.\scripts\hdd\sleep-hdd.ps1

# Take offline before power down (requires Admin)
.\scripts\hdd\sleep-hdd.ps1 -Offline
```

## Hardware Setup

### USB Relay

| Spec | Value |
|------|-------|
| Model | DCT Tech 2-Channel USB HID Relay |
| VID/PID | 0x16C0/0x05DF |
| Interface | USB HID (no drivers needed) |
| Rating | 10A/250VAC, 10A/30VDC |

**Purchase**: [Amazon.ca](https://www.amazon.ca/dp/B0DKBY5YM1) | [Amazon.com](https://www.amazon.com/dp/B0DKBY5YM1)

### Wiring

Connect your HDD's power supply through the relay's normally-open contacts:
- **Relay 1**: 12V line
- **Relay 2**: 5V line (or wire both in series)

When relays are OFF, the drive has no power. When ON, the drive powers up.

## Building from Source

### Requirements

- Windows 10/11
- Visual Studio 2022 with C++ Desktop Development
- Windows SDK 10.0.26100.0

### Build Commands

```batch
# Build the GUI application
scripts\build\compile-gui.bat

# Build command-line utilities
scripts\build\compile.bat

# Build and run tests
scripts\build\compile-tests.bat

# Run tests with coverage report
scripts\build\coverage.bat --open
```

### Project Structure

```
hdd-toggle/
├── src/                    # C/C++ source code
│   ├── hdd-control-gui.cpp # Main tray application
│   ├── relay.c             # USB relay controller
│   ├── wake-hdd.c          # Wake utility
│   └── sleep-hdd.c         # Sleep utility
├── include/                # Headers
│   └── hdd-utils.h         # Shared utilities (100% tested)
├── res/                    # Windows resources
├── assets/                 # Icons and images
├── scripts/
│   ├── build/              # Build scripts
│   ├── hdd/                # PowerShell control scripts
│   └── util/               # Utility scripts
├── tests/                  # Unit tests (Catch2)
├── bin/                    # Build output
└── coverage/               # Coverage reports
```

## Contributing

### Development Setup

1. Clone the repository
2. Open in VS Code or your preferred editor
3. Run `scripts\build\compile-tests.bat` to verify setup

### Code Style

- Modern C++17 (std::filesystem, std::thread, RAII)
- Static linking (`/MT`) for standalone executables
- Minimize external dependencies
- Extract testable pure functions into headers

### Testing

We maintain **100% test coverage** on utility code. Before submitting:

```batch
# Run tests
scripts\build\compile-tests.bat

# Check coverage (requires OpenCppCoverage)
scripts\build\coverage.bat --open
```

Install coverage tool:
```
winget install OpenCppCoverage.OpenCppCoverage
```

### Pull Requests

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass with full coverage
5. Submit PR with clear description

## Troubleshooting

| Issue | Solution |
|-------|----------|
| "USB relay not found" | Check USB connection in Device Manager |
| "Drive not detected" | Run as Administrator, check serial number in INI |
| Tray icon missing after restart | Explorer may not be ready; app retries 10 times |
| Build fails | Verify VS 2022 C++ tools and Windows SDK installed |

### Verify Relay

```cmd
relay.exe all on   # Should hear click
relay.exe all off  # Should hear click
```

### Check Drive Serial

```powershell
wmic diskdrive get model,serialnumber
```

## License

MIT License - See [LICENSE](LICENSE) for details.

## Acknowledgments

- [RemoveDrive](https://www.uwe-sieber.de/drivetools_e.html) by Uwe Sieber - Safe drive ejection
- [Catch2](https://github.com/catchorg/Catch2) - Unit testing framework
- [OpenCppCoverage](https://github.com/OpenCppCoverage/OpenCppCoverage) - Code coverage
