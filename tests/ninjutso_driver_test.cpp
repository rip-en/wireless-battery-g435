// Exercise the production driver with simulated replies; no hardware access.
#include "../src/drivers/drivers.h"

#include <cassert>
#include <cstdio>

namespace {
std::vector<std::vector<BYTE>> replies;
int opens = 0, queries = 0;
bool sendOk = true;

HidColl Collection() {
    HidColl c;
    c.vid = 0x1915;
    c.pid = 0xAE1C;
    c.caps.UsagePage = 0xFFA0;
    c.caps.Usage = 1;
    c.caps.FeatureReportByteLength = 704;
    return c;
}

std::vector<BYTE> Status(BYTE battery, bool charging = false, bool online = true) {
    std::vector<BYTE> r(32, 0);
    r[0] = 5;
    r[1] = 0x15;
    r[9] = battery;
    r[10] = charging;
    r[12] = online;
    return r;
}

PollContext Poll(std::vector<std::vector<BYTE>> data) {
    replies = std::move(data);
    queries = 0;
    PollContext ctx;
    ctx.colls = { Collection() };
    PollNinjutso(ctx);
    return ctx;
}
}  // namespace

bool HidDev::OpenFeatureOnly(const HidColl& c) {
    c_ = c;
    h_ = reinterpret_cast<HANDLE>(1);
    ++opens;
    return true;
}
void HidDev::Close() { h_ = INVALID_HANDLE_VALUE; }
bool HidDev::SetFeature(const std::vector<BYTE>& data) {
    auto expected = std::vector<BYTE>(32, 0);
    expected[0] = 5;
    expected[1] = 0x15;
    expected[4] = 1;
    assert(data == expected);  // only the documented state query can be sent
    ++queries;
    return sendOk;
}
bool HidDev::GetFeature(BYTE id, std::vector<BYTE>& out) {
    assert(id == 5);
    if (replies.empty()) return false;
    out = replies.front();
    replies.erase(replies.begin());
    return true;
}
std::wstring Hex4(USHORT value) {
    wchar_t s[8];
    swprintf(s, 8, L"%04X", value);
    return s;
}

int main() {
    auto ctx = Poll({ Status(32) });
    assert(ctx.out.size() == 1 && ctx.IsClaimed(0x1915, 0xAE1C));
    const auto& s = ctx.out[0];
    assert(s.percent == 32 && s.connected && !s.charging);
    assert(s.name == L"Ninjutso Sora V2" && s.key == L"ninjutso:1915:AE1C");
    assert(s.kind == DeviceKind::Mouse && s.via == L"2.4 GHz receiver");
    assert(queries == 1);
    int before = opens;
    PollNinjutso(ctx);
    assert(opens == before && ctx.out.size() == 1);

    ctx = Poll({ Status(0, true) });
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 100 && ctx.out[0].charging);
    ctx = Poll({ Status(32, true, false), Status(31) });
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 31 && queries == 2);
    ctx = Poll({ Status(0, true, false), Status(0, true, false), Status(0, true, false), Status(0, true, false) });
    assert(ctx.out.size() == 1 && !ctx.out[0].connected && !ctx.out[0].charging && ctx.out[0].percent == -1);
    assert(queries == 4);

    auto wrongId = Status(32), wrongCommand = Status(32);
    wrongId[0] = 4;
    wrongCommand[1] = 0x28;
    ctx = Poll({ wrongId, wrongCommand, std::vector<BYTE>(12), Status(101) });
    assert(ctx.out.empty() && ctx.claimed.empty() && queries == 4);
    ctx = Poll({});
    assert(ctx.out.empty() && ctx.claimed.empty() && queries == 4);
    sendOk = false;
    ctx = Poll({ Status(32) });
    assert(ctx.out.empty() && queries == 1);
    sendOk = true;

    ctx = {};
    auto otherVendor = Collection(), otherProduct = Collection(), otherPage = Collection();
    auto otherUsage = Collection(), shortReport = Collection();
    otherVendor.vid = 0x1234;
    otherProduct.pid = 0xAE8C;  // 8K receiver is outside the verified scope
    otherPage.caps.UsagePage = 1;
    otherUsage.caps.Usage = 2;
    shortReport.caps.FeatureReportByteLength = 31;
    ctx.colls = { otherVendor, otherProduct, otherPage, otherUsage, shortReport };
    before = opens;
    PollNinjutso(ctx);
    assert(opens == before && ctx.out.empty());
    puts("Ninjutso driver tests passed");
}
