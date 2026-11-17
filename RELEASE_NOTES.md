# Wireless Battery (G435) 1.1.0

Lightweight Windows tray app for wireless device battery levels, with **Logitech G435** support for both LIGHTSPEED dongles (`046D:0ACB` and `046D:0ADF`).

Based on [krisgfx9/Wireless-Device-Battery-Monitor](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor) (GPL-3.0).

## Download

- **Portable:** `WirelessBattery-G435-1.1.0-portable.zip` — unzip and run `WirelessBattery.exe`
- Or run `WirelessBattery-G435.exe` directly

Windows may SmartScreen-warn (unsigned). *More info → Run anyway*.

## Quick start

1. Quit any older Wireless Battery / WirelessBattery process
2. Plug in the G435 receiver and power the headset on
3. Run the exe
4. Look for a tray icon (default). Right-click for menu / *Start with Windows*

```bat
WirelessBattery.exe --list
WirelessBattery.exe --dump
```

## G435 notes

- First USB battery read is delayed ~20s after launch; last reading is restored from the registry
- Later reads at most every 45 minutes (*Refresh now* forces one — may briefly mute audio on the Realtek dongle)
- Prefer **tray icons** if the taskbar ever feels sticky

## Build from source

MinGW-w64 (MSYS2): `make`  
Or `build.bat` with `g++` / `windres` on `PATH`.
