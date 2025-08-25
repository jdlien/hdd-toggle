This is a set of simple scripts to control a mechanical hard drive elegantly in windows.

Programming Rules:

- Keep scripts simple
- Minimize dependencies as much as possible
- Use best practices for handling hard drive control

- If you need to run .bat files, like for compiling, you can run them via pwsh or powershell

- Our current focus is making a Windows 11 system tray icon app using C++ to control the hard drive power (essentially by running sleep-hdd.exe and wake-hdd.exe), but there are many challenges to doing this well. We use `compile-gui.bat` to actually perform the compilation of this program.

- We use relay.c (compiled with compile.bat) to physically control the relays that turn HDD power on or off
- wake-hdd.ps1 turns on the relay and ensures that Windows detects the connected hard drive and that it is ready to use
- sleep-hdd.ps1 attempts to safely shut down the hard drive, turn off the relay, and ensure that Windows detects the drive has been disconnected


We are doing some work with icons and .ico files, and for that Image Magick 7 (magick) is handy. Here's the full path for its installation: `C:\Program Files\ImageMagick-7.1.2-Q16-HDRI`