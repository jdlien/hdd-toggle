# HDD Toggle Control System

A Windows-based system for elegantly powering on/off mechanical hard drives using USB relay control.

## Overview

This project provides safe, automated power control for a specific 18TB WDC hard drive (Model: WD181KFGX-68AFPN0, Serial: 2VH7TM9L) through physical relay switching combined with Windows device management.

**Target Drive**: WDC WD181KFGX-68AFPN0 (Serial: 2VH7TM9L)  
**USB Relay**: DCT Tech 2-Channel USB HID Relay  
**Amazon Link**: https://www.amazon.ca/dp/B0DKBY5YM1

## Hardware Requirements

### USB Relay Specifications
- **Model**: DCT Tech 2-Channel USB HID Relay Module
- **VID/PID**: 0x16C0/0x05DF
- **Interface**: USB HID (no drivers required)
- **Relay Type**: 2-channel, 10A/250VAC, 10A/30VDC rated
- **Control Protocol**: HID Feature Reports (9-byte packets)
- **Purchase Link**: [Amazon.ca - B0DKBY5YM1](https://www.amazon.ca/dp/B0DKBY5YM1)

### Wiring Setup
Connect the hard drive's power supply (12V and 5V lines) through the relay's normally-open contacts. Both relays should be wired in series to completely cut power to the drive.

## Files

### Core Executables
- **`relay.exe`** - USB HID relay controller (compiled from relay.c)
- **`wake-hdd.ps1`** - Complete drive wake-up sequence
- **`sleep-hdd.ps1`** - Safe drive shutdown sequence

### Source & Build
- **`relay.c`** - C source code for USB HID relay control
- **`compile.bat`** - Build script using Microsoft Visual Studio C compiler

### Documentation
- **`README.md`** - This file
- **`CLAUDE.md`** - Project development guidelines
- **`tasks/status.md`** - Detailed project status and technical documentation

## Usage

### Prerequisites
- Windows 10/11
- DCT Tech USB relay connected and detected by Windows
- Target hard drive wired through relay contacts
- Microsoft Visual Studio Build Tools (if recompiling)

### Basic Commands

#### Wake Up Drive
```powershell
.\wake-hdd.ps1
```
- Powers up both relays
- Triggers Windows device detection
- Waits for drive to come online
- Reports status when ready

#### Sleep Drive
```powershell
# Standard sleep (hot-unplug compatible)
.\sleep-hdd.ps1

# Take disk offline before power down (safer)
.\sleep-hdd.ps1 -Offline
```

#### Manual Relay Control
```cmd
# Turn on both relays
relay.exe all on

# Turn off both relays  
relay.exe all off

# Control individual relays
relay.exe 1 on
relay.exe 2 off
```

### Command Options

#### sleep-hdd.ps1 Parameters
- **`-Offline`** - Take disk offline before power down (requires Administrator)
- **`-Help`** - Show detailed help information

## Technical Details

### USB Relay Communication
The relay uses standard USB HID interface with these specifications:
- **Report Size**: 9 bytes
- **Report ID**: 0 (first byte)
- **Command Byte** (second byte):
  - `0xFC` = All relays OFF
  - `0xFE` = All relays ON  
  - `0xFD` = Single relay OFF (requires channel number in byte 3)
  - `0xFF` = Single relay ON (requires channel number in byte 3)

### Safety Features
- Serial number verification prevents wrong drive operations
- Status checking before and after operations
- Graceful handling of already online/offline drives
- UAC elevation prompts when administrative access needed
- Comprehensive error reporting

### Windows Integration
- PnP device enumeration and detection
- Disk Management API integration
- Automatic privilege escalation when needed
- Multi-method device detection with fallbacks

## Build Instructions

To rebuild `relay.exe` from source:

```cmd
# Standard build (standalone executable)
compile.bat

# Smaller build (requires VC++ redistributable)  
compile.bat small
```

Requires Microsoft Visual Studio Build Tools or Visual Studio with C++ components.

## Troubleshooting

### Common Issues
1. **"USB relay not found"** - Check USB connection and device manager
2. **"Drive not detected"** - May need administrator privileges, try running PowerShell as admin
3. **Compilation fails** - Ensure Visual Studio Build Tools are installed and accessible

### Verification Steps
1. Test relay clicking: `relay.exe all on` then `relay.exe all off`
2. Check drive detection: Run `wake-hdd.ps1` and observe status messages
3. Verify power control: Monitor drive spin-up/spin-down sounds

## License

This project is provided as-is for educational and personal use.

---
*Hardware-specific implementation for WDC WD181KFGX-68AFPN0 drive control*