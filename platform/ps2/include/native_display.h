#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace ps2::native {
constexpr unsigned frame_width = 496, frame_height = 384;
// GS User Manual v6 section2.5.4: CT32 may pair with Z16S, not Z16.
// Section3.7.4: ZTE stays enabled even when ZMSK disables depth writes.
inline constexpr unsigned framebuffer_storage_format = 0x00; // PSMCT32
inline constexpr unsigned depth_storage_format = 0x3a;       // PSMZ16S
inline constexpr unsigned depth_register_format = 0x0a;
// Exact allocation footprint: two CT32 frames, one Z16S, 128x64 font,
// fourteen256x256 PSMT4 slots and256 block-aligned CLUT slots.
constexpr std::size_t display_vram_bytes(unsigned output_height) noexcept {
    return std::size_t(640) * output_height * 10 + 128 * 64 * 4 + 14 * 32768 + 256 * 256;
}
// Native assets use linear packed PSMT4 indices: left pixel in the low nibble.
// The uploader performs no arcade texture addressing or color translation.
struct Texture {
    const std::uint8_t *indices = nullptr;
    std::size_t bytes = 0;
    std::uint16_t width = 0, height = 0;
    std::uint64_t generation = 0;
};
struct Vertex {
    float x = 0, y = 0, u = 0, v = 0, q = 1;
    // Optional native study occlusion: normalized reverse depth, far0/near1.
    // It is independent of q and does not represent recovered arcade priority.
    float depth = 0;
};
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
    bool depth_test = false; // Opt-in Z16S GREATER_EQUAL; painter order by default.
};
struct Stats {
    std::uint32_t triangles = 0, texture_uploads = 0, texture_hits = 0;
    std::uint32_t palette_uploads = 0, palette_hits = 0;
    std::uint64_t texture_bytes = 0, palette_bytes = 0;
};
enum class ReadbackStatus {
    ok, not_ready, invalid_rectangle, busy, unsupported_configuration,
    gif_timeout, finish_timeout, receive_timeout, fifo_timeout, cleanup_failed
};
// Materials and assets remain caller-owned through present(). Increment the
// texture generation after editing bytes. Before releasing or reusing texture
// storage, call Display::invalidate_textures(): an address and generation can
// otherwise identify a different asset when an allocator recycles that address.
// Width/height: powers of two8..256.
// Vertex q must be positive; abs(u/v) and q are limited to1,000,000.
bool valid_frame(const Frame &frame, const char *&failure);
// Caller passes a validated [0,1] value. Nearest integer, far0 and near65535.
std::uint16_t reverse_depth16(float depth) noexcept;
// Pure PS2 platform renderer. No board, instruction-set or arcade video types.
// Calls and lifetimes are serialized by the caller. One initialized Display
// owns the process-global GS/GIF state and SDK VRAM allocator at a time.
class Display {
public:
    Display();
    ~Display();
    Display(const Display &) = delete;
    Display &operator=(const Display &) = delete;
    // Idempotent after success. Rejection of a second owner touches no hardware.
    // Failed setup releases its CPU buffers/lease so initialization can retry.
    // A retired transport requires process restart, including after destruction.
    bool initialize();
    bool present(const Frame &frame);
    bool present_status(const char *message);
    // Between calls, forget all cached texture identities. The next use uploads
    // its index bytes; copied-content CLUT entries remain valid. No allocation,
    // transfer or stats/error change. present() completes GS work before return.
    void invalidate_textures() noexcept;
    // Optional diagnostic, never used by present(). Reads the completed physical
    // 640 x output_height() framebuffer in row order, raw GS 0xAABBGGRR words
    // (alpha retains the GS0..0x80 convention). No allocation. Byte count must
    // be a multiple of128 and <=65536; e.g.8x8 samples or640x16 strips.
    // Caller exclusively owns GS/GIF/VIF1, with no pending VU1 work, throughout
    // this synchronous call, including no future VU1 XGKICK or other producers.
    // Busy/configuration rejection changes no hardware.
    // Output stays unchanged on failure. A transport timeout retires this
    // Display: restart the diagnostic instead of drawing after a partial read.
    // An incomplete receive quarantines the reverse bus until restart: changing
    // BUSDIR with unread FIFO data would violate the GS hardware contract.
    ReadbackStatus readback_rect(unsigned x, unsigned y, unsigned width,
                                 unsigned height, std::span<std::uint32_t> pixels);
    // XYZ quantization only; projection remains caller-owned. Scalar default.
    void set_vu0_packing(bool enabled);
    // Buffer is caller-owned and must remain alive while enabled.
    void set_debug_page(const char *text);
    // Optional two-line study label; caller retains text while enabled.
    void set_caption(const char *text);
    const char *error() const;
    unsigned output_height() const;
    const Stats &stats() const;
private:
    struct Impl;
    Impl *impl_;
};
} // namespace ps2::native
