// Logitech G435 LIGHTSPEED headset. It does not speak HID++. Two receiver revisions exist:
//
//   046D:0ACB  Realtek PA bridge on feature report 6 of usage page FFC0. Entering the bridge
//              mutes USB audio for ~0.8 s. Protocol from github.com/fr0styJS/g435-lightspeed-battery
//              (MIT) and WirelessBattery PRs #72 / #93.
//
//   046D:0ADF  Holtek USB bridge on usage page FF00. CRC-16/XMODEM reports carry the same PA
//              commands. Protocol from github.com/DsGrilo/G435-Battery-app (MIT). Mode switches
//              are temporary (remote → read → local) and restored after every read.
//
// Both paths only send the battery-read sequence; nothing is written to flash. Reads are cached
// for 30 minutes (5 minutes while the headset is off) because of the audio interruption.
#include "drivers.h"
#include "g435_protocol.h"

#include <atomic>
#include <functional>

namespace {

constexpr USHORT kVid = 0x046D;
constexpr USHORT kPidRealtek = 0x0ACB;
constexpr USHORT kPidHoltek = 0x0ADF;
constexpr ULONGLONG kReadEveryMs = 30ull * 60 * 1000;
constexpr ULONGLONG kRetryEveryMs = 5ull * 60 * 1000;

std::atomic<bool> g_forceRead{ false };

struct Cache {
    bool attempted = false;
    bool haveReading = false;
    int lastMv = 0;
    ULONGLONG lastAttempt = 0;
    USHORT pid = 0;
};
Cache g_cache;

// ---- Realtek (046D:0ACB) --------------------------------------------------------------------

bool RealtekSend(HidDev& d, std::initializer_list<BYTE> frame) {
    std::vector<BYTE> b = { 0x06, 0x04, 0x03, 0x00, (BYTE)frame.size() };
    b.insert(b.end(), frame.begin(), frame.end());
    return d.SetFeature(b);
}

int ReadRealtekMv(HidDev& d) {
    std::vector<BYTE> r;
    int mv = 0;
    if (!d.SetFeature({ 0x06, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00 })) return 0;  // enter bootloader
    Sleep(250);
    d.GetFeature(0x06, r);
    if (RealtekSend(d, { 0x50, 0x41, 0x0E, 0x00, 0x02, 0xE1, 0x01 })) {  // remote mode
        Sleep(150);
        if (RealtekSend(d, { 0x50, 0x41, 0x06, 0x00, 0x01, 0x00, 0x31 })) {  // OTA get battery
            ULONGLONG end = GetTickCount64() + 800;
            while (!mv && GetTickCount64() < end) {
                if (d.GetFeature(0x06, r)) {
                    int parsed = 0;
                    if (G435::ParseRealtekBattery(r.data(), r.size(), parsed) && G435::IsPlausible(parsed)) mv = parsed;
                }
                if (!mv) Sleep(30);
            }
        }
        RealtekSend(d, { 0x50, 0x41, 0x0E, 0x00, 0x02, 0xE1, 0x00 });  // local mode
        Sleep(50);
    }
    d.SetFeature({ 0x06, 0x08, 0x03, 0x00, 0x00, 0x00, 0x00 });  // exit bootloader, always
    Sleep(100);
    return mv;
}

// ---- Holtek (046D:0ADF) ---------------------------------------------------------------------

constexpr BYTE kHidAwWrite = 0x80;
constexpr BYTE kHidPowerSaving = 0x85;
constexpr BYTE kLqGetMode = 0xE0;
constexpr BYTE kLqSetMode = 0xE1;
constexpr BYTE kLqGetConnect = 0xE6;
constexpr BYTE kOtaGetBattery = 0x31;

// Write a Holtek bridge command and wait for the interrupt ACK ('O' / 'F').
bool HoltekWrite(HidDev& d, BYTE hidCmd, const std::vector<BYTE>& payload, bool checkAck) {
    auto report = G435::BuildHoltekReport(hidCmd, payload.data(), payload.size());
    d.Flush();
    if (!d.Write(report)) return false;
    std::vector<BYTE> in;
    int n = d.Read(in, 200);
    if (n < 1) return !checkAck;
    // hidapi strip: for report ID 0, in[0] is the first payload byte ('O' or 'F').
    if (checkAck && in[0] != (BYTE)'O') return false;
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

bool HoltekWait(HidDev& d, int timeoutMs, const std::function<bool(const G435::HoltekFrame&)>& match) {
    ULONGLONG end = GetTickCount64() + (ULONGLONG)timeoutMs;
    while (GetTickCount64() < end) {
        std::vector<BYTE> in;
        // Report ID 0 is stripped by HidDev::Read, so in[0] is the Holtek size byte.
        if (d.Read(in, 100) < 1) continue;
        for (const auto& f : G435::ParseHoltekReport(in.data(), in.size())) {
            if (match(f)) return true;
        }
    }
    return false;
}

int HoltekLqGet(HidDev& d, BYTE cmd, BYTE& transmission) {
    if (!HoltekAw(d, G435::HoltekLq(cmd), transmission)) return -1;
    int value = -1;
    HoltekWait(d, 3000, [&](const G435::HoltekFrame& f) {
        return G435::ParseHoltekLqValue(f, cmd, value);
    });
    return value;
}

bool HoltekSetRemote(HidDev& d, bool remote, BYTE& transmission) {
    if (!HoltekAw(d, G435::HoltekLq(kLqSetMode, { (BYTE)(remote ? 1 : 0) }), transmission)) return false;
    return HoltekLqGet(d, kLqGetMode, transmission) == (remote ? 1 : 0);
}

int ReadHoltekMv(HidDev& d) {
    BYTE transmission = 0;
    // Official updater disables the bridge power-saving bit on every connection.
    HoltekWrite(d, kHidPowerSaving, { 0x01 }, false);

    int link = HoltekLqGet(d, kLqGetConnect, transmission);
    if (link < 0 || (link & 0x08) == 0) return 0;  // headset not linked

    int mode = HoltekLqGet(d, kLqGetMode, transmission);
    bool wasRemote = mode == 1;
    if (!wasRemote && !HoltekSetRemote(d, true, transmission)) return 0;

    int mv = 0;
    BYTE trans = 0;
    if (!HoltekAw(d, G435::HoltekOta(kOtaGetBattery), transmission, &trans)) {
        if (!wasRemote) HoltekSetRemote(d, false, transmission);
        return 0;
    }
    HoltekWait(d, 3000, [&](const G435::HoltekFrame& f) {
        int parsed = 0;
        if (!G435::ParseHoltekOtaMv(f, trans, parsed)) return false;
        if (G435::IsPlausible(parsed)) { mv = parsed; return true; }
        return false;
    });

    if (!wasRemote) HoltekSetRemote(d, false, transmission);
    return mv;
}

}  // namespace

void G435RequestRead() { g_forceRead = true; }

void PollG435(PollContext& ctx) {
    if (ctx.IsClaimed(kVid, kPidRealtek) && ctx.IsClaimed(kVid, kPidHoltek)) return;

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
    if (!coll) {
        if (realtek.empty() && holtek.empty()) g_cache = {};
        return;
    }

    ctx.Claim(kVid, pid);

    ULONGLONG now = GetTickCount64();
    ULONGLONG wait = g_cache.haveReading ? kReadEveryMs : kRetryEveryMs;
    bool force = g_forceRead.exchange(false);
    if (force || !g_cache.attempted || g_cache.pid != pid || now - g_cache.lastAttempt >= wait) {
        HidDev d;
        if (d.Open(*coll)) {
            int mv = isHoltek ? ReadHoltekMv(d) : ReadRealtekMv(d);
            g_cache.attempted = true;
            g_cache.lastAttempt = now;
            g_cache.pid = pid;
            g_cache.haveReading = mv != 0;
            if (mv) g_cache.lastMv = mv;
        }
    }
    if (!g_cache.haveReading) return;

    DeviceStatus s;
    s.key = L"g435:" + Hex4(pid);
    s.name = L"Logitech G435";
    s.via = isHoltek ? L"LIGHTSPEED receiver (Holtek)" : L"LIGHTSPEED receiver";
    s.kind = DeviceKind::Headset;
    s.millivolts = g_cache.lastMv;
    s.percent = G435::Percent(g_cache.lastMv);
    // Keep the last reading visible if a later forced read fails briefly.
    s.graceSec = 1800;
    ctx.out.push_back(s);
}
