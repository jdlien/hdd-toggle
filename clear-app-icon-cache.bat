@echo off
echo Clearing Windows application icon caches for HDD Control...
echo This will help Windows recognize the updated app icon.

echo.
echo Stopping Windows Explorer...
taskkill /f /im explorer.exe >nul 2>&1

echo Clearing icon caches...
del /f /q "%localappdata%\IconCache.db" >nul 2>&1
del /f /q "%localappdata%\Microsoft\Windows\Explorer\iconcache_*.db" >nul 2>&1

echo Clearing thumbnail caches...
del /f /q "%localappdata%\Microsoft\Windows\Explorer\thumbcache_*.db" >nul 2>&1

echo Clearing notification area registry cache...
reg delete "HKCU\Software\Classes\Local Settings\Software\Microsoft\Windows\CurrentVersion\TrayNotify" /f >nul 2>&1

echo Clearing Windows.old icon caches if they exist...
del /f /q "%SystemRoot%\system32\IconCache.db" >nul 2>&1

echo Restarting Windows Explorer...
start explorer.exe

echo.
echo Icon caches cleared! Windows should now use the updated icon.
echo Try running hdd-control-gui-v2.exe and check the toast notifications.
pause