# GitHub release

Final release of this fork, for the original G435 Realtek dongle (`046D:0ACB`).

- Repo: https://github.com/rip-en/wireless-battery-g435
- Release: https://github.com/rip-en/wireless-battery-g435/releases/tag/v1.2.0
- Upstream PR (closed, not merged): https://github.com/krisgfx9/Wireless-Device-Battery-Monitor/pull/189

Upstream main supports `046D:0ADF` and does not read `046D:0ACB`. The reason, and the hardware constraint, is in [g435.md](g435.md) and [UPSTREAM.md](UPSTREAM.md).

## Publish

From a built `WirelessBattery.exe`:

```powershell
Copy-Item WirelessBattery.exe dist\WirelessBattery-G435.exe
# dist\WirelessBattery-G435-1.2.0-portable.zip contains that exe plus LICENSE.txt
git tag -a v1.2.0 -m "Wireless Battery (G435) 1.2.0"
git push origin main
git push origin v1.2.0
gh release create v1.2.0 --title "Wireless Battery (G435) 1.2.0" --notes-file RELEASE_NOTES.md dist\WirelessBattery-G435.exe dist\WirelessBattery-G435-1.2.0-portable.zip
```

Previous release: https://github.com/rip-en/wireless-battery-g435/releases/tag/v1.1.0-g435
