# Changelog

## 1.2.0

Final release for the original G435 Realtek dongle (`046D:0ACB`).

Upstream closed [PR #189](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor/pull/189): main supports `046D:0ADF` and does not read `046D:0ACB`, because a battery read on that dongle interrupts audio. The mute is a property of the Realtek PA bridge (feature report 6, usage page `FFC0`). There is no passive battery channel on that revision.

This build still reads `0ACB`, at most once every 45 minutes, and always exits the bootloader. Behavior of the read path is unchanged from 1.1.0. The version resource now reports 1.2.0, and the docs record the hardware constraint.

## 1.1.0-g435

- Tray menu **Check interval** (30s / 1m / 2m / 5m / 15m), remembered in the registry
- Slightly smaller tray glyphs and taskbar widget padding

- Stabilize G435 percent: median voltage samples + ±2% sticky display so Refresh does not flicker mid-curve. — 2026-11-17

First public build of this fork.

### Added
- Logitech G435 LIGHTSPEED battery support for both receivers:
  - Realtek `046D:0ACB`
  - Holtek `046D:0ADF`
- Registry persistence of the last G435 reading (shows immediately on restart)
- `docs/g435.md` protocol and performance notes

### Changed (vs upstream Wireless Battery)
- Default display is **tray icons** (taskbar widget optional)
- Taskbar widget no longer uses UI Automation (avoids Explorer freezes on Windows 11)
- Process runs at below-normal priority
- Device poll interval 90s; G435 USB reads at most every 45 minutes (deferred ~20s after launch)
- Realtek bootloader path always exits (RAII), including on failure

### Notes
- Based on [krisgfx9/Wireless-Device-Battery-Monitor](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor) (GPL-3.0)
- G435 Realtek protocol also explored upstream in PRs #72 and #93; this fork adds Holtek + runtime hardening validated for responsiveness
