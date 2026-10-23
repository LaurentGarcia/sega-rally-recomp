#pragma once
#include "presentation.h"
#include "model2_gs.h"

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
    // Explicit experimental nearest/base-level GS sampling. No full-frame
    // polygon image upload. Unsupported frames return false before drawing.
    bool present_model2(std::span<const uint32_t> background, std::span<const uint32_t> foreground,
                        int stride, std::span<const app::Model2Frame::PolyVertex> vertices,
                        std::span<const app::Model2Frame::Batch> batches, const rt::VideoMem &memory);
    const NativeGsStats &native_stats() const;
    void set_vu0_packing(bool enabled);
    // Caller owns a persistent NUL-terminated buffer. A fullscreen diagnostic
    // page draws at most40columns/23lines at2x glyph size after real rendering.
    // nullptr disables it, so diagnostics never obscure the game by default.
    void set_debug_page(const char *text);
    const char *error() const;
    int output_height() const;
private:
    struct Impl;
    Impl *impl_;
};
} // namespace ps2
