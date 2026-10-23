// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels, Ville Linde, Aaron Giles
// Texture addressing and texel-center shading adapt runtime/raster.cpp's
// pinned MAME reference (dddd73680656e355bb2b5beecab1167c9f07bf81).
// The GS cache, validation and native presentation are platform code.
#include "model2_gs.h"
#include "app/gpu/model2_resources.h"
#include "runtime/raster.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace ps2 {
namespace {
bool finite(float value) { return (std::bit_cast<uint32_t>(value) & 0x7f800000u) != 0x7f800000u; }
uint16_t le16(const uint8_t *bytes, uint32_t index) {
    return uint16_t(bytes[index * 2] | uint16_t(bytes[index * 2 + 1]) << 8);
}
uint32_t texel(const NativeTexture &texture, unsigned x, unsigned y) {
    // The packed Model 2 sheet's 2048x1024 mapping is stored as 1024x2048.
    unsigned px = texture.x + x, py = texture.y + y;
    if (px >= 1024) { px -= 1024; py ^= 1024; }
    const unsigned offset = (py / 2) * 512 + px / 2;
    uint32_t word = texture.sheet[(offset >> 1) & 0x7ffff];
    if (offset & 1) word >>= 16;
    if (!(y & 1)) word >>= 8;
    if (!(x & 1)) word >>= 4;
    return word & 15;
}
}

bool native_texture(const app::Model2Frame::PolyVertex &v, const rt::VideoMem &memory,
                    NativeTexture &texture) {
    const unsigned state = v.info[0];
    if (!(state & 2) || (state & (1u << 7)) ||
        !memory.lumaram || !memory.colorxlat) return false;
    NativeTexture out;
    out.sheet = state & (1u << 10) ? memory.tex1 : memory.tex0;
    if (!out.sheet) return false;
    out.width = uint16_t(32u << ((state >> 17) & 7));
    out.height = uint16_t(32u << ((state >> 20) & 7));
    out.mirror_x = state & (1u << 5);
    out.mirror_y = state & (1u << 6);
    if (out.upload_width() > kGsTextureSide || out.upload_height() > kGsTextureSide) return false;
    out.x = uint16_t(v.info[1]); out.y = uint16_t(v.info[1] >> 16);
    if (out.x > 2016 || out.y > 992 || (out.x & 31) || (out.y & 31)) return false;
    out.generation = memory.tex_generation;
    const unsigned lumabase = v.info[3] & 0xffff, luma = v.info[3] >> 16;
    if (lumabase > 0x7f80 || (lumabase & 127) || luma > 255 || v.more[1] > 0x7fff) return false;
    const unsigned color = v.more[1];
    const unsigned red = ((color >> 0) & 31) << 8;
    const unsigned green = 0x2000 + (((color >> 5) & 31) << 8);
    const unsigned blue = 0x4000 + (((color >> 10) & 31) << 8);
    static const auto gamma = app::model2_resources::make_gamma();
    for (unsigned index = 0; index < 16; ++index) {
        const unsigned level = std::min(unsigned(memory.lumaram[(lumabase + index * 8) * 4]) * luma / 256, 63u);
        out.colors[index] = 0x80000000u | gamma[le16(memory.colorxlat, red + level) & 255] |
            uint32_t(gamma[le16(memory.colorxlat, green + level) & 255]) << 8 |
            uint32_t(gamma[le16(memory.colorxlat, blue + level) & 255]) << 16;
    }
    // Renderer 3's transparent index. This reproduces nearest texel cutout,
    // not Model 2's filtered transparency threshold at an edge.
    if ((state & 3) == 3) out.colors[15] = 0;
    texture = out;
    return true;
}

bool decode_native_texture(const NativeTexture &texture, std::span<uint32_t> destination) {
    return decode_native_texture_rows(texture, 0, texture.upload_height(), destination);
}

bool decode_native_texture_rows(const NativeTexture &texture, unsigned first_row, unsigned row_count,
                                std::span<uint32_t> destination) {
    const unsigned width = texture.upload_width(), height = texture.upload_height();
    if (!texture.sheet || !texture.width || !texture.height || width > kGsTextureSide ||
        height > kGsTextureSide || first_row > height || row_count > height - first_row ||
        destination.size() < size_t(width) * row_count) return false;
    for (unsigned row = 0; row < row_count; ++row) {
        const unsigned y = first_row + row;
        const unsigned sy = y < texture.height ? y : 2 * texture.height - 1 - y;
        for (unsigned x = 0; x < width; ++x) {
            const unsigned sx = x < texture.width ? x : 2 * texture.width - 1 - x;
            destination[size_t(row) * width + x] = texture.colors[texel(texture, sx, sy)];
        }
    }
    return true;
}

bool decode_native_indices(const NativeTexture &texture, std::span<uint8_t> destination) {
    const unsigned width = texture.upload_width(), height = texture.upload_height();
    if (!texture.sheet || !texture.width || !texture.height || (width & 1) || width > kGsTextureSide ||
        height > kGsTextureSide || destination.size() < size_t(width) * height / 2) return false;
    for (unsigned y = 0; y < height; ++y) {
        const unsigned sy = y < texture.height ? y : 2 * texture.height - 1 - y;
        for (unsigned x = 0; x < width; x += 2) {
            const unsigned sx0 = x < texture.width ? x : 2 * texture.width - 1 - x;
            const unsigned sx1 = x + 1 < texture.width ? x + 1 : 2 * texture.width - 2 - x;
            destination[(size_t(y) * width + x) / 2] = uint8_t(texel(texture, sx0, sy) | texel(texture, sx1, sy) << 4);
        }
    }
    return true;
}

bool native_frame_supported(std::span<const app::Model2Frame::PolyVertex> vertices,
                            std::span<const app::Model2Frame::Batch> batches,
                            const rt::VideoMem &memory, NativeGsStats &stats) {
    stats = {};
    const auto reject = [&](const char *reason) { stats.rejection = reason; return false; };
    size_t next = 0;
    for (const auto &batch : batches) {
        if (batch.first != next || batch.count % 3 || batch.count > vertices.size() - next)
            return reject("INVALID TRIANGLE BATCH");
        if (batch.clip.x < 0 || batch.clip.y < 0 || batch.clip.w <= 0 || batch.clip.h <= 0 ||
            batch.clip.x > kFrameWidth - batch.clip.w || batch.clip.y > kFrameHeight - batch.clip.h)
            return reject("INVALID SCISSOR");
        for (size_t i = next; i < next + batch.count; i += 3) {
            const auto &a = vertices[i];
            if ((a.info[0] & 3) == 1) return reject("UNSUPPORTED SOLID TRANSPARENCY");
            if ((a.info[0] & 2) && (a.info[0] & (1u << 7))) return reject("UNSUPPORTED MICROTEXTURE");
            for (size_t j = i; j < i + 3; ++j) {
                const auto &v = vertices[j];
                if (v.edges[0] || v.edges[1]) return reject("UNSUPPORTED SHADER EDGE PROXY");
                if (!finite(v.x) || !finite(v.y) || !finite(v.depth) || !(v.depth > 0 && v.depth < 1) ||
                    v.x < -kFrameWidth || v.x > 2 * kFrameWidth || v.y < -kFrameHeight || v.y > 2 * kFrameHeight ||
                    !finite(v.r) || !finite(v.g) || !finite(v.b) || !finite(v.a) ||
                    !(v.r >= 0 && v.r <= 1) || !(v.g >= 0 && v.g <= 1) || !(v.b >= 0 && v.b <= 1) || v.a != 1)
                    return reject("INVALID VERTEX");
                if (v.depth != a.depth || v.r != a.r || v.g != a.g || v.b != a.b ||
                    !std::equal(std::begin(v.info), std::end(v.info), std::begin(a.info)) ||
                    !std::equal(std::begin(v.more), std::end(v.more), std::begin(a.more)))
                    return reject("INCONSISTENT TRIANGLE STATE");
                if ((a.info[0] & 2) && (!(v.ooz > 0) || !finite(v.ooz) ||
                    !finite(v.uoz) || !finite(v.voz))) return reject("INVALID PERSPECTIVE COORDINATES");
            }
            if (i && a.depth < vertices[i - 1].depth) return reject("INVALID FIRST OWNER ORDER");
            ++stats.triangles;
            if (a.info[0] & 4) ++stats.checker_triangles;
            if (a.info[0] & 2) {
                NativeTexture texture;
                if (!native_texture(a, memory, texture)) return reject("UNSUPPORTED TEXTURE OR MEMORY");
                ++stats.textured_triangles;
            } else ++stats.solid_triangles;
        }
        next += batch.count;
    }
    return next == vertices.size() || reject("UNBATCHED VERTICES");
}
} // namespace ps2
