# Wireless Battery (G435) 1.2.0

Final release for the original Logitech G435 LIGHTSPEED dongle (`046D:0ACB`).

Upstream [Wireless-Device-Battery-Monitor](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor) supports the newer `046D:0ADF` dongle and does not read `046D:0ACB`. [PR #189](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor/pull/189) was closed because a battery read on the original dongle interrupts audio. That mute is in the receiver.

Based on upstream (GPL-3.0).

## Download

- **Portable:** `WirelessBattery-G435-1.2.0-portable.zip` — unzip and run `WirelessBattery.exe`
- Or run `WirelessBattery-G435.exe` directly

Windows may SmartScreen-warn (unsigned). *More info → Run anyway*.

## Original dongle (`046D:0ACB`)

The battery is reachable only through the Realtek PA bridge on feature report 6 (`FFC0`). Opening that bridge stops USB audio for about a second. There is no passive channel on this revision: no HID++, no standard battery usage, and the hidden `0x000A` push collection carries voltage only.

Without remote mode the dongle answers with a junk value around 102–115. This app rejects that and accepts only 3000–4500 mV.

If the bootloader is left open, audio can stay muted until the dongle is unplugged. This build always exits it.

The practical choice is a one-second mute at most once every 45 minutes, or no battery reading. This release takes the first option. Upstream takes the second.

## Quick start

1. Quit any older Wireless Battery / WirelessBattery process
2. Plug in the G435 receiver and power the headset on
3. Run the exe
4. Look for a tray icon (default). Right-click for the menu / *Start with Windows*

```bat
WirelessBattery.exe --list
WirelessBattery.exe --dump
```

## G435 notes

- First USB battery read is delayed ~20s after launch; the last reading is restored from the registry
- Later reads at most every 45 minutes (*Refresh now* forces one and may mute audio for about a second on the Realtek dongle)
- Prefer **tray icons** if the taskbar ever feels sticky
- The newer `046D:0ADF` dongle is also read here. Upstream’s release is enough if that is the only receiver you have

## Build from source

MinGW-w64 (MSYS2): `make`  
Or `build.bat` with `g++` / `windres` on `PATH`.
