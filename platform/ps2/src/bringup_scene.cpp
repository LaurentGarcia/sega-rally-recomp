#include "bringup_scene.h"
#include "diagnostic_font.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ps2 {
namespace {
void word(uint8_t *data, size_t offset, uint16_t value) {
    data[offset] = uint8_t(value); data[offset + 1] = uint8_t(value >> 8);
}
} // namespace

BringupScene::BringupScene() : polys_(2), pixels_(size_t(kFrameWidth) * kFrameHeight) {
    word(palette_.data(), (0x1000 + 1) * 2, 31 | (12 << 5) | (4 << 10));
    word(palette_.data(), (0x1000 + 2) * 2, 4 | (18 << 5) | (31 << 10));
    for (size_t channel = 0; channel < 3; ++channel)
        for (size_t color = 0; color < 32; ++color)
            for (size_t luma = 0; luma < 256; ++luma)
                word(translation_.data(), channel * 0x4000 + (color * 256 + luma) * 2,
                     uint16_t(64 + (color * 191 + 15) / 31));
    for (size_t i = 0; i < polys_.size(); ++i) {
        auto &poly = polys_[i];
        poly.z = uint16_t(1 + i);
        poly.texheader[3] = uint16_t((i + 1) << 6);
        poly.luma = 252;
        poly.viewport[0] = 24; poly.viewport[1] = 112; poly.viewport[2] = 471; poly.viewport[3] = 240;
        poly.center[1] = 384;
        poly.num_vertices = 3;
    }
}

void BringupScene::rect(int x, int y, int w, int h, uint32_t color) {
    for (int yy = std::max(0, y); yy < std::min(kFrameHeight, y + h); ++yy)
        for (int xx = std::max(0, x); xx < std::min(kFrameWidth, x + w); ++xx)
            pixels_[size_t(yy) * kFrameWidth + size_t(xx)] = color;
}
void BringupScene::text(int x, int y, const char *value, uint32_t color, int scale) {
    for (; *value; ++value, x += 6 * scale) {
        const auto bits = diagnostic::glyph(*value);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (bits[size_t(row)] & (1u << (4 - col))) rect(x + col * scale, y + row * scale, scale, scale, color);
    }
}

void BringupScene::render(uint32_t frame, const PadSnapshot &pad, const rt::Model2AIO::Inputs &cabinet,
                         int gear, const char *pad_status, bool direct_gs) {
    const float shift = float(int(cabinet.analog[0]) - 128) / 4.f;
    const float points[2][3][2] = {{{64, 160}, {328, 184}, {144, 260}}, {{216, 152}, {440, 240}, {224, 264}}};
    for (size_t p = 0; p < 2; ++p)
        for (size_t i = 0; i < 3; ++i) {
            auto &v = polys_[p].v[i];
            v.x = points[p][i][0] + shift; v.y = -points[p][i][1]; v.p[0] = 1.f;
            auto &out = vertices_[p * 3 + i];
            out = {};
            out.x = v.x; out.y = -v.y; out.depth = float(p + 1) / 4.f; out.ooz = out.a = 1.f;
            const auto channel = [](int value) {
                const int translated = 64 + (value * 191 + 15) / 31;
                return float(uint8_t(double(translated - 64) * 255.0 / 191.0)) / 255.f;
            };
            out.r = channel(p == 0 ? 31 : 4);
            out.g = channel(p == 0 ? 12 : 18);
            out.b = channel(p == 0 ? 4 : 31);
        }
    rt::VideoMem memory;
    memory.palram = palette_.data(); memory.colorxlat = translation_.data();
    raster_.render(polys_, 0, memory, 0, 0, 0, kFrameWidth - 1, 0, kFrameHeight - 1);
    reference_hash_ = raster_.hash(0, kFrameWidth - 1, 0, kFrameHeight - 1);
    std::fill(pixels_.begin(), pixels_.end(), 0xff101923u);
    rect(24, 144, 448, 128, 0xff1b2b39u);
    if (!direct_gs) {
        const uint32_t *input = raster_.pixels();
        for (int y = 0; y < kFrameHeight; ++y)
            for (int x = 0; x < kFrameWidth; ++x) {
                const uint32_t value = input[size_t(y) * raster_.stride() + size_t(x)];
                if (value) pixels_[size_t(y) * kFrameWidth + size_t(x)] = value;
            }
    }
    text(24, 24, "SEGA RALLY", 0xffeeeeeeu, 3);
    text(24, 56, "PS2 PORT - HARDWARE BRING-UP", 0xffff963du, 2);
    text(24, 86, "GAME CORE NOT RUNNING - NO GAME DATA LOADED", 0xffeeeeeeu);
    text(24, 100, "SYNTHETIC GEOMETRY - NOT GAMEPLAY", 0xffeeeeeeu);
    text(24, 124, direct_gs ? "DIRECT GS TRIANGLES / GIF DMA" : "SHARED MODEL 2 CPU RASTER / GS UPLOAD", 0xff7dd5ffu);
    text(24, 286, pad_status, pad.connected ? 0xff88e0a0u : 0xffffbd5cu);
    char line[96];
    std::snprintf(line, sizeof line, "STEER %03u  GAS %03u  BRAKE %03u  GEAR %d", cabinet.analog[0], cabinet.analog[1], cabinet.analog[2], gear);
    text(24, 302, line, 0xffeeeeeeu);
    rect(24, 316, 256, 5, 0xff354452u);
    rect(24 + cabinet.analog[0], 314, 2, 9, 0xffff963du);
    text(24, 334, "STICK STEER  CROSS GAS  SQUARE BRAKE  L1/R1 GEAR", 0xffc0cbd5u);
    text(24, 348, "TRIANGLE SWITCHES RENDER TEST  SELECT COIN  START", 0xffc0cbd5u);
    std::snprintf(line, sizeof line, "FRAME %u", unsigned(frame));
    text(360, 368, line, 0xff7d91a1u);
}
} // namespace ps2
