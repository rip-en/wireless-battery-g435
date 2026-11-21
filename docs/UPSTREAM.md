# Upstream vs this fork

Upstream: [krisgfx9/Wireless-Device-Battery-Monitor](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor)

## Decision

Upstream closed [PR #189](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor/pull/189) on 2026-11-21. Main deliberately does not read the G435’s older `046D:0ACB` dongle, because a read there interrupts the headset’s audio. The newer `046D:0ADF` dongle is supported in main.

That interruption is in the Realtek receiver. The battery is only available through the PA bridge on feature report 6 (`FFC0`). Opening the bridge stops USB audio for about a second, and there is no passive battery channel on this revision. Details are in [g435.md](g435.md).

Do not open another `0ACB` pull request. Owners of the original dongle use this fork. Owners of the `0ADF` dongle can use upstream.

## What 1.2.0 keeps

- Realtek `046D:0ACB` reads, at most once every 45 minutes, with the bootloader always exited
- Holtek `046D:0ADF` reads
- Tray icons as the default, no UI Automation in the taskbar widget
- Below-normal process priority, 90s device poll, ~20s startup defer, registry cache of the last G435 reading

## Release

https://github.com/rip-en/wireless-battery-g435/releases/tag/v1.2.0
