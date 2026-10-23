#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace ps2::native {
constexpr unsigned frame_width = 496, frame_height = 384;
// Native assets use linear packed PSMT4 indices: left pixel in the low nibble.
// The uploader performs no arcade texture addressing or color translation.
struct Texture {
    const std::uint8_t *indices = nullptr;
    std::size_t bytes = 0;
    std::uint16_t width = 0, height = 0;
    std::uint64_t generation = 0;
};
struct Vertex { float x = 0, y = 0, u = 0, v = 0, q = 1; };
struct Material {
    const Texture *texture = nullptr;
    // Asset colors use 0xAARRGGBB (alpha0xff opaque,0 transparent).
    // Upload converts channels and the GS's0..0x80 alpha range.
    std::array<std::uint32_t, 16> palette{};
    std::uint32_t color = 0xffffffffu;
    bool cutout = false;
};
struct Triangle { std::array<Vertex, 3> vertices; const Material *material = nullptr; };
struct Frame {
    // Caller supplies back-to-front order. Coordinates are logical496x384;
    // u/v are texel coordinates and q is reciprocal depth for GS STQ.
    std::span<const Triangle> triangles;
    std::uint32_t clear_color = 0xff101923u;
};
struct Stats {
    std::uint32_t triangles = 0, texture_uploads = 0, texture_hits = 0;
    std::uint32_t palette_uploads = 0, palette_hits = 0;
    std::uint64_t texture_bytes = 0, palette_bytes = 0;
};
// Materials and assets remain caller-owned through present(). Increment the
// texture generation after editing bytes. Width/height: powers of two8..256.
// Vertex q must be positive; abs(u/v) and q are limited to1,000,000.
bool valid_frame(const Frame &frame, const char *&failure);
// Pure PS2 platform renderer. No board, instruction-set or arcade video types.
class Display {
public:
    Display();
    ~Display();
    Display(const Display &) = delete;
    Display &operator=(const Display &) = delete;
    bool initialize();
    bool present(const Frame &frame);
    bool present_status(const char *message);
    // XYZ quantization only; projection remains caller-owned. Scalar default.
    void set_vu0_packing(bool enabled);
    // Buffer is caller-owned and must remain alive while enabled.
    void set_debug_page(const char *text);
    const char *error() const;
    unsigned output_height() const;
    const Stats &stats() const;
private:
    struct Impl;
    Impl *impl_;
};
} // namespace ps2::native
