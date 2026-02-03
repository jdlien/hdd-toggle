## GUI Tray Version – Design and Implementation Report

### Overview
Goal: Provide a small Windows notification area (system tray) application that lets a user sleep or wake the target HDD via a right‑click menu, without using CLI windows. The tray app should reflect the current/last known state, handle elevation requirements when needed, and give clear feedback (icon, tooltip, balloon/toast).

### What the current CLI tools do
- **wake-hdd.exe** (`wake-hdd.c`)
  - Detects if the drive is already online using PowerShell `Get-Disk` by serial/model.
  - If offline: turns relays on via `relay.exe all on`, rescans devices (tries elevated `pnputil` + `diskpart`, fallback non-elevated), waits for detection, then ensures the disk is brought online (requires admin), prints status.
- **sleep-hdd.exe** (`sleep-hdd.c`)
  - Locates the target disk via WMI (`Get-CimInstance Win32_DiskDrive`).
  - Tries safe removal with `RemoveDrive.exe` for all logical volumes on the disk (retries).
  - Optionally takes disk offline via `diskpart` when `-offline` is passed (requires admin).
  - Powers relays off via `relay.exe all off`, prints final status.

Both tools:
- Use PowerShell/WMI calls to identify the target by `TARGET_SERIAL`/`TARGET_MODEL`.
- Shell out with `CreateProcess`, optionally hidden, and require elevation for certain operations (online/offline, diskpart rescan/offline).

### Tray app objectives
- **Single-click UX**: Right‑click tray icon opens a menu with context‑aware action: "Sleep Drive" or "Wake Drive" based on detected/last state, plus "Refresh Status", "Start with Windows" (optional), and "Exit".
- **Live status**: Icon and tooltip reflect one of: Awake, Sleeping/Powered Off, Unknown/Transitioning.
- **Silent operation**: Commands run without console windows; progress via tooltip/balloon or small notifications.
- **Safe operations**: Use the existing logic paths and timing (relay on/off, rescans, retries) to avoid data loss.

### Technology choices
1) **Win32 C (tray + shelling to existing EXEs)** – Minimal change; reuse `sleep-hdd.exe`/`wake-hdd.exe`. The tray app calls them with hidden windows and interprets exit codes. Pros: fast to build; minimal duplication. Cons: two EXEs to maintain; slightly less integrated error handling.
2) **Win32 C/C++ (integrate logic)** – Move common logic from `sleep-hdd.c`/`wake-hdd.c` into shared modules and link into a single GUI EXE. Pros: unified binary; richer status/error integration; fewer temp files. Cons: refactor effort (WMI/PowerShell querying, retries, elevation paths).
3) **.NET (WinForms/WPF)** – Quicker UI development; easy tray and toast notifications; P/Invoke or process calls to the current EXEs. Pros: rapid UI; good UX polish. Cons: new runtime dependency; language/toolchain change.

Recommended MVP: Option 1 – a small Win32 C tray app that shells to the existing EXEs with hidden windows. Option 2 can follow as a refactor for tighter integration.

### Core design (MVP – Win32 C tray app)
- **Tray icon**: `Shell_NotifyIcon` with `NOTIFYICONDATA`. Handle `WM_USER+X` for callback, and `WM_CONTEXTMENU`/`WM_RBUTTONUP` to show a `TrackPopupMenu` context menu.
- **Menu items**: "Sleep Drive" or "Wake Drive" (contextual), "Refresh Status", separator, "Exit".
- **State detection**:
  - On startup and on a periodic timer (e.g., every 10–15 seconds with backoff), detect status with a hidden PowerShell command: `Get-Disk` by serial/model; `IsOffline` and presence determine Online/Offline/Unknown.
  - Cache last known state; update icon + tooltip text accordingly.
- **Actions**:
  - Sleep: run `sleep-hdd.exe` with hidden window. Optionally pass `-offline` if desired; configurable setting.
  - Wake: run `wake-hdd.exe` with hidden window.
  - After each action, kick a short polling loop to await stabilized state before updating icon.
- **Elevation**:
  - Keep the tray app non-elevated to avoid constant UAC prompts.
  - When an action requires admin steps, the called EXEs already attempt elevation (e.g., `ShellExecute` with `runas` for rescan). Alternatively, explicitly launch them with `runas` to guarantee elevation when needed.
  - Optional advanced path: create a small elevated helper (or Task Scheduler task with highest privileges) the tray can call to avoid UAC prompts on every use.
- **Notifications**:
  - Show brief balloon/toast (e.g., "Waking drive…", "Drive is online", "Sleeping drive…", "Drive powered off").
  - On errors, show a balloon/toast with a succinct message and an action hint (e.g., "Run as Administrator to bring disk online").

### Configuration and persistence
- Replace compile-time `TARGET_SERIAL`/`TARGET_MODEL` with a small config file (ini/json) or registry keys read by the tray app and passed via env/args to the EXEs (or unify later during refactor).
- Remember last known state and user preferences (e.g., use `-offline` on sleep, polling interval, start with Windows) via registry or a local config file.

### Packaging and dependencies
- Keep `tray-hdd.exe` alongside existing `sleep-hdd.exe`, `wake-hdd.exe`, and the required helpers: `relay.exe`, optionally `RemoveDrive.exe` (or ensure it’s on PATH).
- Ensure PowerShell is invoked with `-NoProfile -ExecutionPolicy Bypass` and all subprocesses are hidden.
- Provide an optional installer or a first-run toggle to add a "Run at startup" entry (registry `Run` key) for the tray app.

### Error handling and edge cases
- Disk not present or slow to enumerate: maintain "Unknown" status, keep retrying with backoff; show user how to troubleshoot.
- Elevation cancelled by user: report failure, state remains unchanged.
- `RemoveDrive.exe` missing: sleep proceeds but warn the user that safe removal was skipped.
- Timeouts (e.g., wake’s multi-step flow): present a final failure notification and leave the icon in an "Unknown" or last state, suggesting manual retry.

### Security considerations
- Avoid running the tray app itself elevated; only elevate the specific operations that require it.
- Validate executable paths before spawning; prefer full paths to co‑located tools to prevent PATH hijacking.
- Hide all child process windows; avoid leaking command lines that contain user paths.

### Implementation tasks
1) Create Win32 C tray skeleton: icon, message loop, context menu, exit.
2) Implement status probe: hidden PowerShell `Get-Disk` by serial/model; parse result; set icon + tooltip.
3) Add polling timer with exponential backoff during transitions (sleep/wake) and steady cadence otherwise.
4) Wire actions: invoke `sleep-hdd.exe`/`wake-hdd.exe` hidden; capture exit codes; update UI and schedule immediate status refresh.
5) Elevation handling: rely on EXEs’ internal elevation; if needed, add explicit `runas` for wake and offline operations.
6) Notifications: show balloon/toast on start, success/failure, and when state changes.
7) Settings storage: serial/model, optional `-offline`, start-with-Windows; read/write registry or config file.
8) Packaging: place `tray-hdd.exe` with existing tools; document `RemoveDrive.exe` requirement and PATH behavior.
9) QA: test with disk present/absent, slow spin-up, failed rescan, cancelled UAC, missing `RemoveDrive.exe`, startup auto‑run.
10) Optional refactor: extract common logic from `sleep-hdd.c`/`wake-hdd.c` into a shared module used by the tray app to reduce process spawning and temp files.

### Rough effort estimate
- MVP (Option 1): 0.5–1.5 days, depending on polish and settings storage.
- Refactor (Option 2): +1–3 days for shared module extraction, richer status, and fewer subprocesses.

### Notes on reuse from current code
- Reuse the existing command invocations and retries by delegating to the EXEs.
- The tray can mirror the CLI status checks (e.g., `Get-Disk`/`Get-CimInstance`) to keep logic consistent.
- Elevation pathways and rescan behavior in `wake-hdd.c` (elevated `pnputil`/`diskpart`) already exist and can be leveraged.
