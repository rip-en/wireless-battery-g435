#include "../src/drivers/g435_protocol.h"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vector>

int main() {
    // Realtek GET_BATTERY fragment embedded in a feature report.
    {
        const std::vector<std::uint8_t> report = {
            0x06, 0x50, 0x49, 0x07, 0x00, 0x06, 0x80, 0x00, 0x00, 0x0F, 0x00, 0x00
        };
        int mv = 0;
        assert(G435::ParseRealtekBattery(report.data(), report.size(), mv));
        assert(mv == 0x0F00);  // 3840
        assert(G435::IsPlausible(mv));
    }
    {
        int mv = 0;
        const std::vector<std::uint8_t> garbage = { 0x06, 0x80, 0x00, 0x66, 0x00 };  // 102 mV local-mode junk
        assert(G435::ParseRealtekBattery(garbage.data(), garbage.size(), mv));
        assert(mv == 102);
        assert(!G435::IsPlausible(mv));
    }
    {
        int mv = 0;
        assert(!G435::ParseRealtekBattery(nullptr, 8, mv));
        const std::vector<std::uint8_t> shortReport = { 0x06, 0x80 };
        assert(!G435::ParseRealtekBattery(shortReport.data(), shortReport.size(), mv));
    }

    // Holtek CRC and report framing.
    {
        const std::uint8_t payload[] = { 0x50, 0x41, 0x06, 0x01, 0x01, 0x00, 0x31 };
        auto report = G435::BuildHoltekReport(0x80, payload, sizeof(payload));
        assert(report.size() == 65);
        assert(report[0] == 0 && report[1] == 0x80 && report[2] == sizeof(payload));
        std::uint16_t stored = (std::uint16_t)(report[3] | (report[4] << 8));
        report[3] = report[4] = 0;
        assert(stored == G435::Crc16Xmodem(report.data(), report.size()));
    }

    // Holtek OTA reply frame: size + PI header + 06 <trans|80> 00 <mv LE>.
    {
        std::vector<std::uint8_t> report = {
            17,
            0x50, 0x49, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00,
            0x06, 0x82, 0x00, 0x32, 0x10
        };
        auto frames = G435::ParseHoltekReport(report.data(), report.size());
        assert(frames.size() == 1);
        int mv = 0;
        assert(G435::ParseHoltekOtaMv(frames[0], 0x02, mv));
        assert(mv == 0x1032);  // 4146
        assert(G435::Percent(mv) == G435::Percent(4146));
    }

    // LQ get-mode reply.
    {
        std::vector<std::uint8_t> report = {
            12,
            0x50, 0x49, 0x0E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
            0xE0, 0x01
        };
        auto frames = G435::ParseHoltekReport(report.data(), report.size());
        int value = -1;
        assert(G435::ParseHoltekLqValue(frames[0], 0xE0, value));
        assert(value == 1);
    }

    assert(G435::Percent(4200) == 100);
    assert(G435::Percent(3300) == 0);
    assert(G435::Percent(3831) >= 50 && G435::Percent(3831) <= 60);

    auto lq = G435::HoltekLq(0xE1, { 0x01 });
    const std::vector<std::uint8_t> expectLq = { 0x50, 0x41, 0x0E, 0x00, 0x02, 0xE1, 0x01 };
    assert(lq == expectLq);
    auto ota = G435::HoltekOta(0x31);
    const std::vector<std::uint8_t> expectOta = { 0x50, 0x41, 0x06, 0x00, 0x01, 0x00, 0x31 };
    assert(ota == expectOta);

    std::puts("G435 protocol tests passed.");
    return 0;
}
