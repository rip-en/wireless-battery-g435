#pragma once
// Pure (I/O-free) helpers for the Logitech G435 LIGHTSPEED receivers:
//   * 046D:0ACB — Realtek / PA bridge on feature report 6 (usage page FFC0)
//   * 046D:0ADF — Holtek USB bridge on usage page FF00 (CRC-16/XMODEM reports)
// Voltage → % uses a generic 3.7 V Li-ion curve; Logitech publishes none for the G435.
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace G435 {

inline int Percent(int mv) {
    static const int v[] = { 4200, 4100, 4000, 3900, 3800, 3750, 3700, 3650, 3600, 3500, 3300 };
    static const int p[] = { 100, 90, 80, 65, 50, 40, 30, 20, 10, 5, 0 };
    constexpr int n = 11;
    if (mv >= v[0]) return p[0];
    for (int i = 1; i < n; i++) {
        if (mv >= v[i]) {
            int span = v[i - 1] - v[i];
            return p[i] + (p[i - 1] - p[i]) * (mv - v[i]) / (span ? span : 1);
        }
    }
    return p[n - 1];
}

// Mid-curve (around 3.7–3.8 V) a few mV of ADC noise moves the estimate by 1–2%. Keep the
// previously shown percent unless the new raw value moved by more than `slack`.
inline int StickyPercent(int mv, int prevPct, int slack = 2) {
    int raw = Percent(mv);
    if (prevPct < 0 || slack < 0) return raw;
    if (raw >= prevPct - slack && raw <= prevPct + slack) return prevPct;
    return raw;
}

// Real headset cells sit around 3.0–4.5 V. Local-mode garbage from the Realtek dongle is ~102.
inline bool IsPlausible(int mv) { return mv >= 3000 && mv <= 4500; }

// Realtek GET_BATTERY reply payload fragment: 06 80 00 <mV u16 LE>.
inline bool ParseRealtekBattery(const std::uint8_t* data, std::size_t size, int& millivolts) {
    if (!data) return false;
    for (std::size_t i = 0; i + 5 <= size; i++) {
        if (data[i] == 0x06 && data[i + 1] == 0x80 && data[i + 2] == 0x00) {
            millivolts = (int)(data[i + 3] | (data[i + 4] << 8));
            return true;
        }
    }
    return false;
}

// Holtek CRC-16/XMODEM over a report with the CRC field zeroed (bytes 3–4).
inline std::uint16_t Crc16Xmodem(const std::uint8_t* data, std::size_t size) {
    std::uint16_t crc = 0;
    for (std::size_t i = 0; i < size; i++) {
        crc ^= (std::uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? (std::uint16_t)((crc << 1) ^ 0x1021) : (std::uint16_t)(crc << 1);
    }
    return crc;
}

// Holtek output report (report ID 0): [0, hid_cmd, len, crc_lo, crc_hi, data...] padded to 65 bytes.
inline std::vector<std::uint8_t> BuildHoltekReport(std::uint8_t hidCmd, const std::uint8_t* payload, std::size_t payloadLen) {
    std::vector<std::uint8_t> pkt(65, 0);
    pkt[0] = 0;
    pkt[1] = hidCmd;
    pkt[2] = (std::uint8_t)payloadLen;
    for (std::size_t i = 0; i < payloadLen && i < 60; i++) pkt[5 + i] = payload[i];
    std::uint16_t crc = Crc16Xmodem(pkt.data(), pkt.size());
    pkt[3] = (std::uint8_t)(crc & 0xFF);
    pkt[4] = (std::uint8_t)(crc >> 8);
    return pkt;
}

inline std::vector<std::uint8_t> HoltekLq(std::uint8_t cmd, std::initializer_list<std::uint8_t> params = {}) {
    std::vector<std::uint8_t> p = { 0x50, 0x41, 0x0E, 0x00, (std::uint8_t)(1 + params.size()), cmd };
    p.insert(p.end(), params);
    return p;
}

inline std::vector<std::uint8_t> HoltekOta(std::uint8_t cmd) {
    return { 0x50, 0x41, 0x06, 0x00, 0x01, 0x00, cmd };
}

// Holtek input: [size] + frames of 'P''I' | type | seq | ts u32 LE | len u16 LE | payload[len].
struct HoltekFrame {
    std::uint8_t type = 0;
    std::uint8_t seq = 0;
    std::vector<std::uint8_t> payload;
};

inline std::vector<HoltekFrame> ParseHoltekReport(const std::uint8_t* data, std::size_t size) {
    std::vector<HoltekFrame> frames;
    if (!data || size < 1) return frames;
    std::size_t n = data[0];
    if (n + 1 > size) n = size > 0 ? size - 1 : 0;
    const std::uint8_t* body = data + 1;
    std::size_t i = 0;
    while (i + 10 <= n && body[i] == 0x50 && body[i + 1] == 0x49) {
        std::uint16_t len = (std::uint16_t)(body[i + 8] | (body[i + 9] << 8));
        HoltekFrame f;
        f.type = body[i + 2];
        f.seq = body[i + 3];
        if (i + 10 + len <= n) f.payload.assign(body + i + 10, body + i + 10 + len);
        frames.push_back(std::move(f));
        i += 10 + len;
    }
    return frames;
}

// OTA reply payload: 06 <transmission|0x80> 00 <mV u16 LE>.
inline bool ParseHoltekOtaMv(const HoltekFrame& f, std::uint8_t transmission, int& millivolts) {
    const auto& p = f.payload;
    if (p.size() < 5 || p[0] != 0x06) return false;
    if ((p[1] & 0x7F) != (transmission & 0x7F) || p[2] != 0) return false;
    millivolts = (int)(p[3] | (p[4] << 8));
    return true;
}

inline bool ParseHoltekLqValue(const HoltekFrame& f, std::uint8_t cmd, int& value) {
    const auto& p = f.payload;
    if (p.size() < 2 || p[0] != cmd) return false;
    value = (int)p[1];
    return true;
}

}  // namespace G435
