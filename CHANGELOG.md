# Changelog

All notable changes to HDD Control will be documented in this file.

## [1.1.0] - 2025-02-03

### Fixed
- Fixed startup failure when launching via Task Scheduler at logon
  - Added retry logic to `CreateTrayIcon()` - retries up to 10 times with 1-second delays
  - Resolves issue where `Shell_NotifyIcon` would fail if Explorer's notification area wasn't ready yet
  - App previously exited with error code 1 when tray icon creation failed

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
