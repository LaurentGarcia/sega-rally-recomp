// Deterministic CPU compositor differential: normal/window/split modes,
// line scrolling, both categories, transparent pens, wraps and RAM changes.
#include "runtime/video.h"
#include <cstdio>
#include <vector>

static void word(std::vector<uint8_t> &memory, unsigned offset, uint16_t value) {
    memory.at(offset * 2) = uint8_t(value);
    memory.at(offset * 2 + 1) = uint8_t(value >> 8);
}

int main() {
    uint32_t random = 0x19a7943;
    auto next = [&] { random ^= random << 13; random ^= random >> 17; random ^= random << 5; return random; };
    std::vector<uint8_t> tiles(0x10000), chars(0x80000), palette(0x4000), translation(0xc000), luma(0x20000);
    std::vector<uint32_t> texture(0x80000);
    for (auto &b : tiles) b = uint8_t(next());
    for (auto &b : chars) b = uint8_t(next());
    for (auto &b : palette) b = uint8_t(next());
    for (auto &b : translation) b = uint8_t(next());
    const rt::VideoMem memory{palette.data(), translation.data(), luma.data(), texture.data(), texture.data(), 0};
    rt::Video video(tiles.data(), chars.data());
    const std::vector<rt::GeoPoly> polygons;
    for (unsigned i = 0; i < 4096; ++i) video.palette_w(i, palette.data(), translation.data());
    unsigned frame = 0;
    for (unsigned mode = 0; mode < 4; ++mode)
        for (bool line : {false, true})
            for (unsigned phase = 0; phase < 4; ++phase)
                for (bool wide : {false, true}) {
                    if (phase == 2) video.enable_write_tracking();
                    for (unsigned layer = 0; layer < 4; ++layer) {
                        word(tiles, 0x5000 + layer, uint16_t((line ? 0x8000 : 0) | ((layer * 163 + phase * 113) & 0x3ff)));
                        word(tiles, 0x5004 + layer, uint16_t((mode << 13) | ((phase * 193 + layer * 127) & 0x3ff)));
                    }
                    if (phase == 3) word(tiles, 0x5005, 0x8000); // disabled paired layer
                    for (unsigned i = 0; i < 32; ++i) {
                        chars[next() % chars.size()] ^= uint8_t(next());
                        tiles[next() % 0x8000] ^= uint8_t(next());
                    }
                    video.tile_memory_w(); video.character_memory_w();
                    video.set_wide_margin(wide ? 40 : 0);
                    video.frame_start(); video.screen_update(polygons, 0, memory);
                    std::printf("%u,%016llx\n", ++frame, static_cast<unsigned long long>(video.screen_hash()));
                }
}
