#pragma once
#include "presentation.h"
#include <array>

namespace rt { struct VideoMem; }
namespace ps2 {
// Experimental GS sampling: nearest base-level texels after the Model 2
// palette/luma/gamma lookup. GS filtering cannot reproduce filtering indices
// before that nonlinear lookup, per-pixel mip blending or shader edge rules.
constexpr unsigned kGsTextureSide = 256;
// Four-bit texels and independent16-entry CLUTs share the previous512KiB
// RGBA cache budget. Palette/luma changes never invalidate texture indices.
constexpr unsigned kGsTextureSlots = 14;
constexpr unsigned kGsPaletteSlots = 256;
constexpr size_t kGsUploadBytes = 64 * 1024;
struct NativeGsStats {
    uint32_t triangles = 0, textured_triangles = 0, solid_triangles = 0;
    uint32_t checker_triangles = 0;
    uint32_t texture_uploads = 0, texture_hits = 0;
    uint32_t palette_uploads = 0, palette_hits = 0;
    uint64_t texture_bytes = 0, palette_bytes = 0, layer_bytes = 0;
    const char *rejection = "NONE";
};
struct NativeTexture {
    const uint32_t *sheet = nullptr;
    uint64_t generation = 0;
    uint16_t x = 0, y = 0, width = 0, height = 0;
    bool mirror_x = false, mirror_y = false;
    std::array<uint32_t, 16> colors{};
    unsigned upload_width() const { return width * (mirror_x ? 2u : 1u); }
    unsigned upload_height() const { return height * (mirror_y ? 2u : 1u); }
    bool operator==(const NativeTexture &) const = default;
    bool same_indices(const NativeTexture &other) const {
        return sheet == other.sheet && generation == other.generation && x == other.x && y == other.y &&
            width == other.width && height == other.height && mirror_x == other.mirror_x && mirror_y == other.mirror_y;
    }
};

// Complete preflight before any framebuffer or cache mutation. Unsupported
// microtexture/edge packets, invalid geometry and oversized textures
// require the caller's complete CPU reference fallback, never partial drawing.
bool native_frame_supported(std::span<const app::Model2Frame::PolyVertex> vertices,
                            std::span<const app::Model2Frame::Batch> batches,
                            const rt::VideoMem &memory, NativeGsStats &stats);
bool native_texture(const app::Model2Frame::PolyVertex &vertex, const rt::VideoMem &memory,
                    NativeTexture &texture);
// Tightly packed GS PSMCT32 texels. No allocation; capacity checked first.
bool decode_native_texture(const NativeTexture &texture, std::span<uint32_t> destination);
bool decode_native_texture_rows(const NativeTexture &texture, unsigned first_row, unsigned row_count,
                                std::span<uint32_t> destination);
// Linear PSMT4 host transfer: left pixel occupies the low nibble. The GS
// performs VRAM swizzling during GIF upload; shading remains in its CLUT.
bool decode_native_indices(const NativeTexture &texture, std::span<uint8_t> destination);
} // namespace ps2
