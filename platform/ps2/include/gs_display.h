#pragma once
#include "presentation.h"

namespace ps2 {
class GsDisplay {
public:
    GsDisplay();
    ~GsDisplay();
    GsDisplay(const GsDisplay &) = delete;
    GsDisplay &operator=(const GsDisplay &) = delete;
    bool initialize();
    bool present_rgba(std::span<const uint32_t> source, int stride);
    // Original diagnostic font drawn as GS sprites; no frame-sized allocation.
    bool present_status(const char *message);
    // Draw the reference/background image, then native opaque GS triangles.
    // Vertices and batches follow the shared Model2Frame packet contract.
    bool present_flat(std::span<const uint32_t> background, int stride,
                      std::span<const app::Model2Frame::PolyVertex> vertices,
                      std::span<const app::Model2Frame::Batch> batches);
    const char *error() const;
    int output_height() const;
private:
    struct Impl;
    Impl *impl_;
};
} // namespace ps2
