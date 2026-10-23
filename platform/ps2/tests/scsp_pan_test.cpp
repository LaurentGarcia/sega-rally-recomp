// Exhaustive gain identity against the original independent full lookup.
#include "runtime/scsp_pan_table.h"
#include "runtime/snd_cpu.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

int main() {
    snd::ScspPanTable table;
    std::array<int, 65536> left{}, right{};
    constexpr float sends[8] = {-1000000, -36, -30, -24, -18, -12, -6, 0};
    for (unsigned encoded = 0; encoded < 65536; ++encoded) {
        const unsigned tl = encoded & 255, pan = (encoded >> 8) & 31, send = encoded >> 13;
        float db = 0;
        constexpr float level[8] = {0.4f, 0.8f, 1.5f, 3, 6, 12, 24, 48};
        for (unsigned bit = 0; bit < 8; ++bit) if (tl & (1u << bit)) db -= level[bit];
        const float total = powf(10, db / 20);
        db = 0;
        for (unsigned bit = 0; bit < 4; ++bit) if (pan & (1u << bit)) db -= float(3u << bit);
        const float attenuation = (pan & 15) == 15 ? 0 : powf(10, db / 20);
        const float gain = send ? powf(10, sends[send] / 20) : 0;
        left[encoded] = int((4.0f * (pan < 16 ? attenuation : 1.0f) * total * gain) * 4096);
        right[encoded] = int((4.0f * (pan < 16 ? 1.0f : attenuation) * total * gain) * 4096);
        table.set(uint16_t(encoded), left[encoded], right[encoded]);
    }
    uint64_t hash = 14695981039346656037ull;
    for (unsigned encoded = 0; encoded < 65536; ++encoded) {
        if (table.left(encoded) != left[encoded] || table.right(encoded) != right[encoded])
            throw std::runtime_error("SCSP table collision changed exact gain");
        for (int value : {table.left(encoded), table.right(encoded)})
            for (unsigned shift = 0; shift < 32; shift += 8) {
                hash ^= uint8_t(unsigned(value) >> shift); hash *= 1099511628211ull;
            }
    }
    struct Bus final : snd::AddressSpace {
        uint16_t read16(uint32_t, uint16_t) override { return 0x1234; }
        void write16(uint32_t, uint16_t, uint16_t) override {}
    } bus;
    snd::Cpu68k legacy, board(&bus);
    if (legacy.ram.size() != 65536 || !board.ram.empty() || board.bus != &bus)
        throw std::runtime_error("legacy CPU RAM ownership changed");
    std::printf("pan values=131072 hash=%016llx bytes=%zu legacy_ram=%zu board_ram=%zu\n",
                (unsigned long long)hash, sizeof(table), legacy.ram.size(), board.ram.size());
}
