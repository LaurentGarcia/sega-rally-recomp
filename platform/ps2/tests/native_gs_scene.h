#pragma once
#include "model2_gs.h"
#include "runtime/raster.h"
#include <vector>

// Original ROM-free scene shared by portable decoder checks and the GS ELF.
namespace ps2::test {
struct NativeScene {
    std::vector<uint32_t> sheet = std::vector<uint32_t>(0x80000);
    std::vector<uint8_t> luma = std::vector<uint8_t>(0x20000);
    std::vector<uint8_t> translation = std::vector<uint8_t>(0xc000);
    std::vector<uint32_t> background = std::vector<uint32_t>(kFrameWidth * kFrameHeight, 0xff202040u);
    std::vector<uint32_t> foreground = std::vector<uint32_t>(kFrameWidth * kFrameHeight);
    std::array<app::Model2Frame::PolyVertex, 9> vertices{};
    std::array<app::Model2Frame::Batch, 1> batches{{{0, 9, {0, 0, kFrameWidth, kFrameHeight}}}};
    rt::VideoMem memory{};
    NativeScene() {
        memory.tex0 = sheet.data(); memory.tex_generation = 1;
        memory.lumaram = luma.data(); memory.colorxlat = translation.data();
        for (unsigned i = 0; i < 16; ++i) luma[(i * 8) * 4] = uint8_t(i * 4);
        for (unsigned table = 0; table < 3; ++table)
            for (unsigned i = 0; i < 64; ++i)
                translation[(table * 0x2000 + i) * 2] = uint8_t(64 + i * 3);
        for (unsigned y = 0; y < 32; ++y) for (unsigned x = 0; x < 32; ++x) {
            const unsigned value = x < 8 ? 0 : x < 16 ? 4 : x < 24 ? 8 : 15;
            const unsigned offset = (y / 2) * 512 + x / 2;
            const unsigned shift = ((offset & 1) ? 16 : 0) + ((y & 1) ? 0 : 8) + ((x & 1) ? 0 : 4);
            sheet[offset / 2] |= value << shift;
        }
        const auto vertex = [](float x, float y, float depth, unsigned state, float q, float u, float v) {
            app::Model2Frame::PolyVertex result{};
            result.x = x; result.y = y; result.depth = depth;
            result.ooz = q; result.uoz = u * q; result.voz = v * q;
            result.r = 1; result.g = .125f; result.b = .0625f; result.a = 1;
            result.info[0] = state; result.info[3] = 255u << 16;
            return result;
        };
        // First owner is red checker; far opaque texture remains visible in
        // the holes. The separate right triangle tests transparent texel 15.
        vertices[0] = vertex(80, 80, .2f, 4, 1, 0, 0);
        vertices[1] = vertex(260, 80, .2f, 4, 1, 0, 0);
        vertices[2] = vertex(80, 260, .2f, 4, 1, 0, 0);
        vertices[3] = vertex(300, 40, .5f, 3, 1, 0, 0);
        vertices[4] = vertex(480, 40, .5f, 3, .5f, 32, 0);
        vertices[5] = vertex(300, 220, .5f, 3, 1, 0, 32);
        vertices[6] = vertex(40, 40, .8f, 2, 1, 0, 0);
        vertices[7] = vertex(280, 40, .8f, 2, .5f, 32, 0);
        vertices[8] = vertex(40, 280, .8f, 2, 1, 0, 32);
        for (int y = 20; y < 36; ++y) for (int x = 20; x < 36; ++x)
            foreground[size_t(y) * kFrameWidth + x] = 0xffffffffu;
    }
};
} // namespace ps2::test
