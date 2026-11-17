// Logitech G435 LIGHTSPEED headset. It does not speak HID++. Two receiver revisions exist:
//
//   046D:0ACB  Realtek PA bridge on feature report 6 of usage page FFC0. Entering the bridge
//              mutes USB audio for ~0.8 s. Enter, PA, and exit are HidD_SetFeature (report 6,
//              64 bytes), matching github.com/fr0styJS/g435-lightspeed-battery (MIT) and
//              WirelessBattery PR #93.
//
//   046D:0ADF  Holtek USB bridge on usage page FF00. CRC-16/XMODEM reports carry the same PA
//              commands. Protocol from github.com/DsGrilo/G435-Battery-app (MIT). The 'O'/'F'
//              ack is HidD_GetInputReport (report 0, byte 1), not an interrupt read.
//
// Both paths only send the battery-read sequence; nothing is written to flash. A successful
// read is cached for 45 minutes. A failed read (headset off) is retried after 5 minutes and the
// previous value is shown as asleep. The Realtek path ALWAYS exits the bootloader (RAII) after
// a successful enter — leaving it open wedges USB. The Holtek path restores local mode the same
// way, and the whole Holtek attempt stops within 4 seconds.
// The last good reading is remembered in the registry so a restart can show a value before the
// deferred first USB query runs.
#include "drivers.h"
#include "g435_protocol.h"

#include <algorithm>
#include <atomic>
#include <functional>

namespace {

constexpr USHORT kVid = 0x046D;
constexpr USHORT kPidRealtek = 0x0ACB;
constexpr USHORT kPidHoltek = 0x0ADF;
constexpr ULONGLONG kReadEveryMs = 45ull * 60 * 1000;
constexpr ULONGLONG kRetryEveryMs = 5ull * 60 * 1000;
constexpr ULONGLONG kStartupDeferMs = 20ull * 1000;  // no USB bridge traffic until the desktop settles
constexpr BYTE kReport = 0x06;
constexpr wchar_t kRegKey[] = L"Software\\WirelessBattery";

std::atomic<bool> g_forceRead{ false };
const ULONGLONG g_processStart = GetTickCount64();

struct Cache {
    bool haveReading = false;
    bool lastReadOk = false;    // false -> 5 min retry and show the previous value as asleep
    bool loaded = false;
    int lastMv = 0;
    int lastPct = -1;           // sticky display % so ADC noise does not flicker the tray
    ULONGLONG lastAttempt = 0;  // cooldown; never cleared on brief USB dropouts
    USHORT pid = 0;
};
Cache g_cache;

void LoadPersisted() {
    if (g_cache.loaded) return;
    g_cache.loaded = true;
    DWORD mv = 0, pid = 0, n = sizeof(DWORD);
    if (RegGetValueW(HKEY_CURRENT_USER, kRegKey, L"G435Mv", RRF_RT_REG_DWORD, nullptr, &mv, &n) != ERROR_SUCCESS) return;
    n = sizeof(DWORD);
    if (RegGetValueW(HKEY_CURRENT_USER, kRegKey, L"G435Pid", RRF_RT_REG_DWORD, nullptr, &pid, &n) != ERROR_SUCCESS) return;
    if (!G435::IsPlausible((int)mv) || (pid != kPidRealtek && pid != kPidHoltek)) return;
    g_cache.lastMv = (int)mv;
    g_cache.lastPct = G435::Percent((int)mv);
    g_cache.pid = (USHORT)pid;
    g_cache.haveReading = true;
}

void SavePersisted() {
    if (!g_cache.haveReading) return;
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    DWORD mv = (DWORD)g_cache.lastMv, pid = g_cache.pid;
    RegSetValueExW(key, L"G435Mv", 0, REG_DWORD, (const BYTE*)&mv, sizeof(mv));
    RegSetValueExW(key, L"G435Pid", 0, REG_DWORD, (const BYTE*)&pid, sizeof(pid));
    RegCloseKey(key);
}

// ---- Realtek (046D:0ACB) — feature report 6, same bytes as fr0styJS / #93 --------------------

bool RealtekSendPa(HidDev& d, std::initializer_list<BYTE> frame) {
    std::vector<BYTE> msg = { kReport, 0x04, 0x03, 0x00, (BYTE)frame.size() };
    msg.insert(msg.end(), frame.begin(), frame.end());
    return d.SetFeature(msg);  // padded to the 64-byte feature report
}

// RAII: always leave the bootloader after a successful enter, even when a query fails.
struct BootloaderGuard {
    HidDev& d;
    bool active = false;
    ~BootloaderGuard() {
        if (!active) return;
        d.SetFeature({ kReport, 0x08, 0x03, 0x00, 0x00, 0x00, 0x00 });
        Sleep(100);
    }
};

int ReadRealtekMv(HidDev& d) {
    // Enter is a feature report. A failed SetFeature did not enter, so there is nothing to exit.
    if (!d.SetFeature({ kReport, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00 })) return 0;
    BootloaderGuard guard{ d, true };
    Sleep(250);

    std::vector<BYTE> r;
    d.GetFeature(kReport, r);  // one drain, same as fr0styJS / #93

    if (!RealtekSendPa(d, { 0x50, 0x41, 0x0E, 0x00, 0x02, 0xE1, 0x01 })) return 0;  // remote
    Sleep(150);

    // Several samples while the bridge is open; median beats a single noisy ADC hit.
    // The window matches #93 (800 ms, 30 ms between polls).
    std::vector<int> samples;
    if (RealtekSendPa(d, { 0x50, 0x41, 0x06, 0x00, 0x01, 0x00, 0x31 })) {
        ULONGLONG end = GetTickCount64() + 800;
        while (samples.size() < 5 && GetTickCount64() < end) {
            if (d.GetFeature(kReport, r)) {
                int parsed = 0;
                if (G435::ParseRealtekBattery(r.data(), r.size(), parsed) && G435::IsPlausible(parsed))
                    samples.push_back(parsed);
            }
            if (samples.size() >= 5 || GetTickCount64() >= end) break;
            DWORD nap = (DWORD)(end - GetTickCount64());
            Sleep(nap > 30 ? 30 : nap);
        }
    }

    RealtekSendPa(d, { 0x50, 0x41, 0x0E, 0x00, 0x02, 0xE1, 0x00 });  // local
    Sleep(50);
    if (samples.empty()) return 0;
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];  // guard exits bootloader
}

// ---- Holtek (046D:0ADF) ---------------------------------------------------------------------

constexpr BYTE kHidAwWrite = 0x80;
constexpr BYTE kHidPowerSaving = 0x85;
constexpr BYTE kLqGetMode = 0xE0;
constexpr BYTE kLqSetMode = 0xE1;
constexpr BYTE kLqGetConnect = 0xE6;
constexpr BYTE kOtaGetBattery = 0x31;

bool HoltekWrite(HidDev& d, BYTE hidCmd, const std::vector<BYTE>& payload, bool checkAck) {
    auto report = G435::BuildHoltekReport(hidCmd, payload.data(), payload.size());
    // Drop stale interrupt frames only. The ack itself is a control GET_REPORT: DsGrilo reads
    // get_input_report(0, 65) and checks byte 1. Interrupt Read() strips report ID 0, so the
    // 'O' would be missed, and that read can consume the event frame that carries the reply.
    d.Flush();
    if (!d.Write(report)) return false;
    Sleep(10);
    std::vector<BYTE> in;
    if (!d.GetInputReport(0, in, 65) || in.size() < 2) return false;
    if (checkAck && in[1] != (BYTE)'O') return false;
    return true;
}

bool HoltekAw(HidDev& d, std::vector<BYTE> cmd, BYTE& transmission, BYTE* usedTransmission = nullptr) {
    if (cmd.size() >= 4 && cmd[3] == 0) {
        transmission = (BYTE)(transmission % 127 + 1);
        cmd[3] = transmission;
    }
    if (usedTransmission && cmd.size() >= 4) *usedTransmission = cmd[3];
    return HoltekWrite(d, kHidAwWrite, cmd, true);
}

int MsLeft(ULONGLONG deadline) {
    LONGLONG left = (LONGLONG)deadline - (LONGLONG)GetTickCount64();
    return left > 0 ? (int)left : 0;
}

bool HoltekWait(HidDev& d, ULONGLONG deadline, const std::function<bool(const G435::HoltekFrame&)>& match) {
    while (MsLeft(deadline) > 0) {
        int slice = MsLeft(deadline);
        if (slice > 100) slice = 100;
        std::vector<BYTE> in;
        if (d.Read(in, slice) < 1) {
            if (MsLeft(deadline) > 20) Sleep(20);  // never busy-spin if Read fails immediately
            continue;
        }
        for (const auto& f : G435::ParseHoltekReport(in.data(), in.size())) {
            if (match(f)) return true;
        }
    }
    return false;
}

// Each reply wait is at most 1.5 s, and never past `deadline` (one budget for the whole read).
int HoltekLqGet(HidDev& d, BYTE cmd, BYTE& transmission, ULONGLONG deadline) {
    if (MsLeft(deadline) < 1) return -1;
    if (!HoltekAw(d, G435::HoltekLq(cmd), transmission)) return -1;
    int value = -1;
    ULONGLONG waitEnd = GetTickCount64() + 1500;
    if (waitEnd > deadline) waitEnd = deadline;
    HoltekWait(d, waitEnd, [&](const G435::HoltekFrame& f) {
        return G435::ParseHoltekLqValue(f, cmd, value);
    });
    return value;
}

bool HoltekSetRemote(HidDev& d, bool remote, BYTE& transmission, ULONGLONG deadline) {
    if (MsLeft(deadline) < 1) return false;
    if (!HoltekAw(d, G435::HoltekLq(kLqSetMode, { (BYTE)(remote ? 1 : 0) }), transmission)) return false;
    return HoltekLqGet(d, kLqGetMode, transmission, deadline) == (remote ? 1 : 0);
}

int ReadHoltekMv(HidDev& d) {
    // One absolute deadline for the attempt, including the local-mode restore. A silent
    // headset must not stall the poll past this (CONTRIBUTING's 4-second cap).
    const ULONGLONG hardEnd = GetTickCount64() + 4000;
    const ULONGLONG queryEnd = hardEnd - 800;  // leave time to send local mode
    BYTE transmission = 0;
    if (!HoltekWrite(d, kHidPowerSaving, { 0x01 }, false)) return 0;

    int link = HoltekLqGet(d, kLqGetConnect, transmission, queryEnd);
    if (link < 0 || (link & 0x08) == 0) return 0;

    int mode = HoltekLqGet(d, kLqGetMode, transmission, queryEnd);
    if (mode < 0) return 0;  // don't switch mode if we never learned the current one
    bool wasRemote = mode == 1;

    struct LocalModeGuard {
        HidDev& dev;
        BYTE& transmission;
        ULONGLONG deadline;
        bool armed = false;
        ~LocalModeGuard() {
            if (armed) HoltekSetRemote(dev, false, transmission, deadline);
        }
    } restore{ d, transmission, hardEnd };

    if (!wasRemote) {
        restore.armed = true;  // set-remote can succeed even when the verify step does not
        if (!HoltekSetRemote(d, true, transmission, queryEnd)) return 0;
    }

    int mv = 0;
    BYTE trans = 0;
    if (!HoltekAw(d, G435::HoltekOta(kOtaGetBattery), transmission, &trans)) return 0;
    ULONGLONG waitEnd = GetTickCount64() + 1500;
    if (waitEnd > queryEnd) waitEnd = queryEnd;
    HoltekWait(d, waitEnd, [&](const G435::HoltekFrame& f) {
        int parsed = 0;
        if (!G435::ParseHoltekOtaMv(f, trans, parsed)) return false;
        if (G435::IsPlausible(parsed)) { mv = parsed; return true; }
        return false;
    });
    return mv;  // guard restores local mode when this read switched into remote
}

}  // namespace

void G435RequestRead() { g_forceRead = true; }

void PollG435(PollContext& ctx) {
    LoadPersisted();

    // Feature report 6 only — #93's match. The OUT pipe is not used.
    auto realtek = ctx.Find(kVid, kPidRealtek, [](const HidColl& c) {
        return c.page() == 0xFFC0 && c.featLen() == 64;
    });
    auto holtek = ctx.Find(kVid, kPidHoltek, [](const HidColl& c) {
        return c.page() == 0xFF00 && c.outLen() >= 65 && c.inLen() >= 64;
    });

    const HidColl* coll = nullptr;
    USHORT pid = 0;
    bool isHoltek = false;
    if (!realtek.empty() && !ctx.IsClaimed(kVid, kPidRealtek)) {
        coll = realtek[0];
        pid = kPidRealtek;
    } else if (!holtek.empty() && !ctx.IsClaimed(kVid, kPidHoltek)) {
        coll = holtek[0];
        pid = kPidHoltek;
        isHoltek = true;
    }
    if (!coll) return;  // keep cooldown across brief USB dropouts (bootloader re-enumerate)

    ctx.Claim(kVid, pid);

    ULONGLONG now = GetTickCount64();
    // A failed read must not keep the 45-minute schedule just because an older value exists.
    ULONGLONG wait = (g_cache.haveReading && g_cache.lastReadOk) ? kReadEveryMs : kRetryEveryMs;
    bool force = g_forceRead.exchange(false);
    bool startupHold = !force && g_cache.lastAttempt == 0 && now - g_processStart < kStartupDeferMs;
    bool due = !startupHold && (force || g_cache.lastAttempt == 0 || g_cache.pid != pid || now - g_cache.lastAttempt >= wait);
    if (due) {
        HidDev d;
        if (d.Open(*coll)) {
            // Record the attempt BEFORE the read so a DEVICECHANGE mid-read cannot start another.
            g_cache.lastAttempt = now;
            g_cache.pid = pid;
            int mv = isHoltek ? ReadHoltekMv(d) : ReadRealtekMv(d);
            if (mv) {
                g_cache.haveReading = true;
                g_cache.lastReadOk = true;
                g_cache.lastMv = mv;
                g_cache.lastPct = G435::StickyPercent(mv, g_cache.lastPct);
                SavePersisted();
            } else {
                g_cache.lastReadOk = false;
            }
        }
    }
    if (!g_cache.haveReading) return;

    DeviceStatus s;
    s.key = L"g435:" + Hex4(pid);
    s.name = L"Logitech G435";
    s.via = isHoltek ? L"LIGHTSPEED receiver (Holtek)" : L"LIGHTSPEED receiver";
    s.kind = DeviceKind::Headset;
    s.millivolts = g_cache.lastMv;
    if (g_cache.lastPct < 0) g_cache.lastPct = G435::StickyPercent(g_cache.lastMv, -1);
    s.percent = g_cache.lastPct;
    s.graceSec = 1800;  // receiver unplugged: UI keeps this value, dimmed, for 30 minutes
    // Persisted or failed readings stay visible, but dimmed until a live read succeeds.
    s.asleep = startupHold || !g_cache.lastReadOk;
    ctx.out.push_back(s);
}
