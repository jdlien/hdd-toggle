# Changelog

All notable changes to HDD Toggle will be documented in this file.

## [3.0.1] - 2026-02-03

### Added
- **ARM64 support**: Native Windows ARM64 build (`hdd-toggle-arm64.exe`)
- Build script now accepts architecture parameter: `compile-gui.bat [x64|arm64]`
- CI builds both x64 and ARM64 binaries

### Fixed
- **Startup task naming**: Renamed scheduled task from "HDD Control Startup" to "HDD Toggle Startup" for consistency with project rename
- Updated `check-startup.bat` and `remove-startup.bat` to use new task name
- `remove-startup.bat` now removes both old and new task names for backward compatibility

## [3.0.0] - 2026-02-03

### Breaking Changes
- **Unified binary**: All 4 executables consolidated into single `hdd-toggle.exe`
  - `hdd-control.exe` → `hdd-toggle.exe` (or `hdd-toggle gui`)
  - `relay.exe on/off` → `hdd-toggle relay on/off`
  - `wake-hdd.exe` → `hdd-toggle wake`
  - `sleep-hdd.exe` → `hdd-toggle sleep`
- Config file `[Commands]` section removed (wake/sleep now internal)

### Added
- **CLI interface**: New subcommand-based CLI
  - `hdd-toggle wake` - Power on drive
  - `hdd-toggle sleep [--offline]` - Power off drive
  - `hdd-toggle relay <on|off>` - Control all relays
  - `hdd-toggle relay <1|2> <on|off>` - Control single relay
  - `hdd-toggle status [--json]` - Show drive status (new!)
  - `hdd-toggle --help` - Show usage
  - `hdd-toggle --version` - Show version
- **JSON status output**: `hdd-toggle status --json` for scripting
- **Shared core modules**: Extracted common code into `src/core/`
- **New file structure**: Organized source into `src/gui/`, `src/commands/`, `src/core/`

### Changed
- Converted C utilities (relay.c, wake-hdd.c, sleep-hdd.c) to C++
- GUI now calls internal functions instead of spawning processes
- Simplified installation - only one executable to deploy
- Updated AUMID for toast notifications

### Removed
- Separate executables (relay.exe, wake-hdd.exe, sleep-hdd.exe, hdd-control.exe)
- `WakeCommand` and `SleepCommand` config options (now hardcoded internally)

## [2.0.0] - 2025-02-03

### Added
- **Modern C++17 codebase** - Replaced legacy C patterns with std::filesystem, std::thread, RAII
- **Windows 11 dark mode support** - Context menus now follow system theme
- **WinRT toast notifications** - Modern Windows 11-style notifications instead of legacy balloons
- **Unit test framework** - Added Catch2 tests for utility functions
- **Comprehensive documentation** - New CLAUDE.md with build instructions and architecture details
- **Install/uninstall scripts** - Simple batch scripts for installation to %LOCALAPPDATA%\HDD-Toggle with Start Menu shortcut and optional elevated startup
- **Example config file** - hdd-control.ini.example with documented settings

### Changed
- **Project restructured** for maintainability:
  - `src/` - C/C++ source files
  - `include/` - Header files
  - `res/` - Windows resources (.rc, .manifest)
  - `assets/` - Icons, images, source artwork
  - `scripts/build/` - Compilation scripts
  - `scripts/hdd/` - PowerShell control scripts
  - `scripts/util/` - Utility scripts
  - `bin/` - Build output
- **Encapsulated global state** - All runtime state now in AppState struct
- **RAII patterns for COM** - Automatic cleanup with ComPtr and ComInitializer
- **Build scripts auto-detect paths** - Can run from project root or scripts/build/

### Fixed
- Fixed startup failure when launching via Task Scheduler at logon
  - Added retry logic to `CreateTrayIcon()` - retries up to 10 times with 1-second delays
  - Resolves issue where `Shell_NotifyIcon` would fail if Explorer's notification area wasn't ready yet
- Fixed menu toggle behavior (clicking tray icon again closes menu instead of reopening)

## [1.0.2] - 2025-08-25

### Added
- Initial versioned release
- System tray application for HDD power control
- USB relay integration for physical power switching
- Drive state detection (on/off/unknown)
- Context menu with Wake, Sleep, and Refresh options
- Toast notifications for status changes
- Configurable settings via `hdd-control.ini`
- Task Scheduler integration for elevated startup without UAC prompts
- DPI awareness for modern displays
- Explorer restart handling (auto-recreates tray icon)
