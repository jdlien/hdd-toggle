# Quick Win: Async Operations Fix

## The Problem
Currently in `hdd-control-gui.cpp`, operations block the UI:

```cpp
// Line 517-526 - This BLOCKS the UI thread!
int result = ExecuteCommand("wake-hdd.exe", TRUE);
if (result == 0) {
    ShowBalloonTip("", "Drive wake completed", NIIF_INFO);
} else {
    ShowBalloonTip("", "Drive wake failed", NIIF_WARNING);
}
```

The UI becomes unresponsive for 3-10 seconds during wake/sleep operations.

## Simple Thread-Based Solution

Add this structure and function to `hdd-control-gui.cpp`:

```cpp
// Add near line 45 with other globals
struct AsyncOperationData {
    HWND hwnd;
    BOOL isWake;  // TRUE for wake, FALSE for sleep
};

// Add this new function around line 500
DWORD WINAPI AsyncDriveOperation(LPVOID lpParam) {
    AsyncOperationData* data = (AsyncOperationData*)lpParam;
    
    const char* exe = data->isWake ? "wake-hdd.exe" : "sleep-hdd.exe";
    const char* operation = data->isWake ? "wake" : "sleep";
    
    // Execute the command
    int result = ExecuteCommand(exe, TRUE);
    
    // Post message back to main thread for UI updates
    PostMessage(data->hwnd, WM_COMMAND, 
                data->isWake ? IDM_WAKE_COMPLETE : IDM_SLEEP_COMPLETE, 
                (LPARAM)result);
    
    delete data;
    return 0;
}

// Add these new command IDs near line 32
#define IDM_WAKE_COMPLETE 1005
#define IDM_SLEEP_COMPLETE 1006
```

Then modify the wake/sleep handlers:

```cpp
// Replace OnWakeDrive (lines 508-527)
void OnWakeDrive() {
    if (g_isTransitioning) return;
    
    g_isTransitioning = TRUE;
    g_driveState = DS_TRANSITIONING;
    UpdateTrayIcon();
    ShowBalloonTip("", "Waking drive...", NIIF_INFO);
    
    // Create thread for async operation
    AsyncOperationData* data = new AsyncOperationData{g_hWnd, TRUE};
    CreateThread(NULL, 0, AsyncDriveOperation, data, 0, NULL);
}

// Replace OnSleepDrive (lines 529-548)
void OnSleepDrive() {
    if (g_isTransitioning) return;
    
    g_isTransitioning = TRUE;
    g_driveState = DS_TRANSITIONING;
    UpdateTrayIcon();
    ShowBalloonTip("", "Sleeping drive...", NIIF_INFO);
    
    // Create thread for async operation
    AsyncOperationData* data = new AsyncOperationData{g_hWnd, FALSE};
    CreateThread(NULL, 0, AsyncDriveOperation, data, 0, NULL);
}
```

Add handlers for completion messages in WindowProc:

```cpp
// Add these cases in WindowProc switch statement (around line 107)
case IDM_WAKE_COMPLETE:
    if (lParam == 0) {
        ShowBalloonTip("", "Drive wake completed", NIIF_INFO);
    } else {
        ShowBalloonTip("", "Drive wake failed", NIIF_WARNING);
    }
    SetTimer(g_hWnd, IDT_STATUS_TIMER, 3000, NULL);
    break;
    
case IDM_SLEEP_COMPLETE:
    if (lParam == 0) {
        ShowBalloonTip("", "Drive sleep completed", NIIF_INFO);
    } else {
        ShowBalloonTip("", "Drive sleep failed", NIIF_WARNING);
    }
    SetTimer(g_hWnd, IDT_STATUS_TIMER, 3000, NULL);
    break;
```

## Even Better: Add Progress Animation

For visual feedback during operations, add an animated icon:

```cpp
// Add near globals (line 66)
UINT_PTR g_animationTimer = 0;
int g_animationFrame = 0;

// Add animation timer ID
#define IDT_ANIMATION_TIMER 2002

// Add this function
void StartProgressAnimation() {
    g_animationFrame = 0;
    g_animationTimer = SetTimer(g_hWnd, IDT_ANIMATION_TIMER, 500, NULL);
}

void StopProgressAnimation() {
    if (g_animationTimer) {
        KillTimer(g_hWnd, IDT_ANIMATION_TIMER);
        g_animationTimer = 0;
    }
    UpdateTrayIcon();  // Restore normal icon
}

// In WindowProc WM_TIMER handler, add:
case IDT_ANIMATION_TIMER:
    {
        // Animate tooltip text with dots
        char tooltip[128];
        const char* dots[] = {"", ".", "..", "..."};
        g_animationFrame = (g_animationFrame + 1) % 4;
        
        sprintf_s(tooltip, sizeof(tooltip), "HDD Control - %s%s", 
                  g_isTransitioning ? "Working" : "Ready",
                  dots[g_animationFrame]);
        
        strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), tooltip);
        g_nid.uFlags = NIF_TIP;
        Shell_NotifyIcon(NIM_MODIFY, &g_nid);
    }
    break;
```

Then call `StartProgressAnimation()` in OnWakeDrive/OnSleepDrive and `StopProgressAnimation()` in the completion handlers.

## Benefits

1. **UI stays responsive** - Menu remains clickable, can show "Exit" option
2. **Visual feedback** - User sees progress indication
3. **Minimal code changes** - ~50 lines of code
4. **No new dependencies** - Uses standard Windows threads
5. **Backward compatible** - No changes to external scripts

## Compile and Test

```batch
compile-gui.bat
```

Then test by clicking wake/sleep - the menu should remain responsive!

## Future Improvements

Once this works, consider:
1. Add a "Cancel" option during operations
2. Show operation progress percentage
3. Queue multiple operations instead of blocking
4. Use std::thread for modern C++ approach