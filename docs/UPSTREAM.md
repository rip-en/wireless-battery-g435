# Upstream vs this fork

Upstream: [krisgfx9/Wireless-Device-Battery-Monitor](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor)

## What to contribute upstream

**Yes — open a PR for G435 support.** Upstream already wants this ([issue #86](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor/issues/86); open PRs #72 / #93 cover Realtek `0ACB` only).

Ideal upstream PR should be **G435-only**:

| File | Why |
|---|---|
| `src/drivers/g435.cpp` | Dual-receiver driver (Realtek + Holtek), safe bootloader exit, rate limit |
| `src/drivers/g435_protocol.h` | Pure protocol helpers |
| `tests/g435_protocol_test.cpp` | Unit tests |
| `src/drivers/drivers.h` | `PollG435` / `G435RequestRead` declarations |
| `src/devices.cpp` | Call `PollG435` before `PollLogitech` |
| `src/main.cpp` | Wire *Refresh now* → `G435RequestRead()` only |
| `README.md` | Device table + Limitations + Credits |
| `docs/g435.md` | Protocol notes |
| `Makefile` | Add protocol test to `make test` |

In the PR description:

- Credit #72, #93, [fr0styJS/g435-lightspeed-battery](https://github.com/fr0styJS/g435-lightspeed-battery), [DsGrilo/G435-Battery-app](https://github.com/DsGrilo/G435-Battery-app)
- Say which receiver you tested (`0ACB` / `0ADF`)
- Paste `WirelessBattery.exe --list` and `--dump` lines for `046D:`
- Note Realtek reads mute audio ~1s; 45‑minute cache is intentional
- Follow CONTRIBUTING: read-only queries, no `Co-authored-by` AI lines

## What to keep in your fork / release

These are **opinionated** and may not belong in the first upstream PR:

- Tray icons as default
- Removing UI Automation from the taskbar widget
- Below-normal process priority
- 90s global poll interval / 20s startup defer / registry cache for G435

Ship those here as a “G435-friendly / lighter UI” build. If upstream likes them later, send separate small PRs.

## Recommended workflow

1. **Publish this fork** under your GitHub account with a release (portable zip + exe).
2. **Fork upstream**, cherry-pick or re-apply only the G435 driver files onto their `main`, open one focused PR.
3. Link your release in the upstream issue/PR so G435 owners can test immediately while review happens.
