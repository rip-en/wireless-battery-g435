# GitHub release (done)

Published under **rip-en**:

- Repo: https://github.com/rip-en/wireless-battery-g435
- Release: https://github.com/rip-en/wireless-battery-g435/releases/tag/v1.1.0-g435
- Upstream PR: https://github.com/krisgfx9/Wireless-Device-Battery-Monitor/pull/189
- Upstream fork: https://github.com/rip-en/Wireless-Device-Battery-Monitor

Note: GitHub Actions workflows were omitted from the GitHub `main` push because the connected OAuth token lacks the `workflow` scope. The source tree and release binaries are complete.

---

# Publish on your GitHub

Suggested repo name: `wireless-battery-g435`

```bash
# 1. Create an empty public repo on GitHub (no README/license — this tree already has them)
#    https://github.com/new

# 2. From this project:
git remote add github https://github.com/YOUR_USER/wireless-battery-g435.git
git push -u github main
git push github v1.1.0-g435

# 3. GitHub → Releases → Draft a new release
#    Tag: v1.1.0-g435
#    Title: Wireless Battery (G435) 1.1.0
#    Body: paste RELEASE_NOTES.md
#    Attach:
#      - WirelessBattery-G435-1.1.0-portable.zip
#      - WirelessBattery-G435.exe
```

Portable zip and exe are also in this agent’s artifacts folder after the build.

## Then contribute upstream

```bash
# Fork krisgfx9/Wireless-Device-Battery-Monitor on GitHub, then:
git clone https://github.com/YOUR_USER/Wireless-Device-Battery-Monitor.git
cd Wireless-Device-Battery-Monitor
git checkout -b feat/logitech-g435-dual-receiver

# Copy only the G435-focused files from this fork (see docs/UPSTREAM.md),
# commit, push, open a PR against krisgfx9/main.
# Mention issue #86 and PRs #72 / #93; include --list / --dump from your headset.
```
