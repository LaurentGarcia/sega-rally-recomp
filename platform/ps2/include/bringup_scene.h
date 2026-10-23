#pragma once
#include "controls.h"
#include "presentation.h"
#include "runtime/raster.h"
#include <array>
#include <vector>

namespace ps2 {
// Original synthetic geometry. Exercises the existing reference raster and
// the direct GS packet boundary without containing or executing game code.
class BringupScene {
public:
    BringupScene();
    void render(uint32_t frame, const PadSnapshot &pad, const rt::Model2AIO::Inputs &cabinet,
                int gear, const char *pad_status, bool direct_gs);
    std::span<const uint32_t> pixels() const { return pixels_; }
    std::span<const app::Model2Frame::PolyVertex> vertices() const { return vertices_; }
    std::span<const app::Model2Frame::Batch> batches() const { return batches_; }
    uint64_t reference_hash() const { return reference_hash_; }
private:
    rt::Raster raster_;
    std::array<uint8_t, 0x4000> palette_{};
    std::array<uint8_t, 0xc000> translation_{};
    std::vector<rt::GeoPoly> polys_;
    std::vector<uint32_t> pixels_;
    std::array<app::Model2Frame::PolyVertex, 6> vertices_{};
    std::array<app::Model2Frame::Batch, 1> batches_{{{0, 6, {24, 144, 448, 128}}}};
    uint64_t reference_hash_ = 0;
    void rect(int x, int y, int w, int h, uint32_t color);
    void text(int x, int y, const char *value, uint32_t color, int scale = 1);
};
} // namespace ps2
