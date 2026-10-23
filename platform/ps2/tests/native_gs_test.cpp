#include "native_gs_scene.h"
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace {
void check(bool passed, const char *message) { if (!passed) throw std::runtime_error(message); }
}
int main() {
    try {
        ps2::test::NativeScene scene;
        ps2::NativeGsStats stats;
        check(ps2::native_frame_supported(scene.vertices, scene.batches, scene.memory, stats), "original scene accepted");
        check(stats.triangles == 3 && stats.textured_triangles == 2 && stats.solid_triangles == 1 &&
              stats.checker_triangles == 1, "complete primitive statistics");
        ps2::NativeTexture opaque, cutout;
        check(ps2::native_texture(scene.vertices[6], scene.memory, opaque), "opaque texture decoded");
        check(ps2::native_texture(scene.vertices[3], scene.memory, cutout), "cutout texture decoded");
        check(opaque.width == 32 && opaque.height == 32 && opaque.colors[15] != 0 && cutout.colors[15] == 0,
              "transparent index isolated from opaque mode");
        check(opaque.same_indices(cutout), "opaque and cutout palettes share invariant indices");
        check(opaque.colors[0] == 0x80000000u && opaque.colors[4] == 0x803c3c3cu && opaque.colors[8] == 0x807c7c7cu,
              "texel-center luma translation and gamma");
        std::array<uint32_t, 1024> decoded;
        decoded.fill(0xdeadbeefu);
        check(!ps2::decode_native_texture(opaque, {decoded.data(), decoded.size() - 1}) && decoded[0] == 0xdeadbeefu,
              "short output rejected before mutation");
        scene.sheet[0] = 0x01234567u;
        check(ps2::decode_native_texture(opaque, decoded), "bounded texture decode");
        check(decoded[0] == opaque.colors[4] && decoded[1] == opaque.colors[5] &&
              decoded[32] == opaque.colors[6] && decoded[33] == opaque.colors[7] &&
              decoded[2] == opaque.colors[0] && decoded[3] == opaque.colors[1] &&
              decoded[34] == opaque.colors[2] && decoded[35] == opaque.colors[3], "packed sheet nibble addressing");
        opaque.x = 1024;
        scene.sheet[0x20000] = 0x89abcdefu;
        ps2::decode_native_texture(opaque, decoded);
        check(decoded[0] == opaque.colors[12] && decoded[1] == opaque.colors[13] &&
              decoded[32] == opaque.colors[14], "sheet second half maps to y bit10");
        opaque.x = 0; opaque.mirror_x = opaque.mirror_y = true;
        std::array<uint32_t, 4096> mirrored;
        check(ps2::decode_native_texture(opaque, mirrored), "mirrored period decode");
        check(mirrored[0] == mirrored[63] && mirrored[0] == mirrored[63 * 64] &&
              mirrored[31] == mirrored[32] && mirrored[31 * 64] == mirrored[32 * 64], "mirror period endpoints");
        ps2::NativeTexture original;
        ps2::native_texture(scene.vertices[6], scene.memory, original);
        ++scene.memory.tex_generation;
        ps2::NativeTexture changed;
        ps2::native_texture(scene.vertices[6], scene.memory, changed);
        check(!(original == changed), "texture write generation invalidates cache key");
        check(!original.same_indices(changed), "texture write invalidates index cache");
        scene.memory.tex_generation = original.generation;
        scene.luma[4 * 8 * 4] = 0;
        ps2::native_texture(scene.vertices[6], scene.memory, changed);
        check(!(original == changed), "changed shade lookup invalidates cache key without texture write");
        check(original.same_indices(changed), "changed shade reuses index cache");
        for (unsigned width : {32u, 64u, 128u, 256u}) {
            for (unsigned height : {32u, 64u, 128u, 256u}) {
                for (unsigned mirror = 0; mirror < 4; ++mirror) {
                    auto indexed = original;
                    indexed.width = uint16_t(width); indexed.height = uint16_t(height);
                    indexed.mirror_x = mirror & 1; indexed.mirror_y = mirror & 2;
                    if (indexed.upload_width() > 256 || indexed.upload_height() > 256) continue;
                    const size_t pixels = size_t(indexed.upload_width()) * indexed.upload_height();
                    std::vector<uint8_t> indices(pixels / 2, 0xcd);
                    std::vector<uint32_t> rgba(pixels);
                    check(!ps2::decode_native_indices(indexed, {indices.data(), indices.size() - 1}) && indices[0] == 0xcd,
                          "short indexed output rejected before writes");
                    check(ps2::decode_native_indices(indexed, indices) && ps2::decode_native_texture(indexed, rgba),
                          "bounded indexed and RGBA decode");
                    for (size_t pixel = 0; pixel < pixels; ++pixel)
                        check(indexed.colors[(indices[pixel / 2] >> ((pixel & 1) * 4)) & 15] == rgba[pixel],
                              "indexed CLUT lookup exactly reconstructs every reference texel and mirror");
                }
            }
        }
        auto large = original;
        large.width = large.height = 256;
        std::vector<uint32_t> full(256 * 256), strips(full.size());
        check(ps2::decode_native_texture(large, full), "large texture reference decode");
        for (unsigned rows_per_strip : {1u, 7u, 32u, 64u, 128u}) {
            std::fill(strips.begin(), strips.end(), 0xdeadbeefu);
            for (unsigned first = 0; first < 256; first += rows_per_strip) {
                const unsigned rows = std::min(rows_per_strip, 256 - first);
                check(ps2::decode_native_texture_rows(large, first, rows,
                    {strips.data() + first * 256, size_t(rows) * 256}), "partial texture decode");
            }
            check(full == strips, "strip decoding equals complete image for every row");
        }
        strips[0] = 0xdeadbeefu;
        check(!ps2::decode_native_texture_rows(large, 250, 7, strips) && strips[0] == 0xdeadbeefu,
              "invalid strip range rejected before writes");
        const auto saved = scene.vertices;
        scene.vertices[6].info[0] |= 1u << 7;
        check(!ps2::native_frame_supported(scene.vertices, scene.batches, scene.memory, stats), "microtexture fallback");
        scene.vertices = saved;
        for (unsigned i = 6; i < 9; ++i) scene.vertices[i].info[0] |= 4u << 17;
        check(!ps2::native_frame_supported(scene.vertices, scene.batches, scene.memory, stats), "oversized texture fallback");
        scene.vertices = saved; scene.vertices[0].edges[1] = 1;
        check(!ps2::native_frame_supported(scene.vertices, scene.batches, scene.memory, stats), "shader edge proxy fallback");
        scene.vertices = saved; scene.vertices[7].ooz = 0;
        check(!ps2::native_frame_supported(scene.vertices, scene.batches, scene.memory, stats), "invalid perspective fallback");
        scene.vertices = saved; scene.vertices[0].x = std::numeric_limits<float>::quiet_NaN();
        check(!ps2::native_frame_supported(scene.vertices, scene.batches, scene.memory, stats), "invalid coordinate fallback");
        scene.vertices = saved; scene.vertices[8].more[1] = 1;
        check(!ps2::native_frame_supported(scene.vertices, scene.batches, scene.memory, stats), "inconsistent state fallback");
        scene.vertices = saved; scene.batches[0].first = 1;
        check(!ps2::native_frame_supported(scene.vertices, scene.batches, scene.memory, stats), "noncontiguous batch fallback");
        scene.batches[0].first = 0; scene.batches[0].clip.x = -1;
        check(!ps2::native_frame_supported(scene.vertices, scene.batches, scene.memory, stats), "invalid scissor fallback");
        std::puts("NATIVE GS portable decoder/preflight PASS; GS image and hardware untested by this test.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Native GS test failed: %s\n", error.what());
        return 1;
    }
}
