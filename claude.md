This is a set of simple scripts to control a mechanical hard drive elegantly in windows.

Programming Rules:

- Keep scripts simple
- Minimize dependencies as much as possible
- Use best practices for handling hard drive control

- We use relay.c (compiled with compile.bat) to physically control the relays that turn HDD power on or off
- wake-hdd.ps1 turns on the relay and ensures that Windows detects the connected hard drive and that it is ready to use
- sleep-hdd.ps1 attempts to safely shut down the hard drive, turn off the relay, and ensure that Windows detects the drive has been disconnected