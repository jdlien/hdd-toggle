# Changelog

All notable changes to HDD Control will be documented in this file.

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
