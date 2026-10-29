#include "native_display.h"
#include "native_depth_checks.h"
#include <bit>
#include <cassert>
#include <limits>
#include <cstdio>
#include <vector>

int main() {
    using namespace ps2::native;
    static_assert(framebuffer_storage_format == 0x00 && depth_storage_format == 0x3a && depth_register_format == 0x0a);
    static_assert(display_vram_bytes(448) == 3424256 && display_vram_bytes(512) == 3833856);
    static_assert(display_vram_bytes(512) < 4 * 1024 * 1024);
    assert(reverse_depth16(0) == 0 && reverse_depth16(1) == 65535);
    assert(reverse_depth16(.25f) == 16384 && reverse_depth16(.5f) == 32768 && reverse_depth16(.75f) == 49151);
    unsigned previous = 0;
    for (unsigned value = 0; value <= 65536; ++value) {
        const auto encoded = reverse_depth16(float(value) / 65536.f);
        assert(encoded >= previous); previous = encoded;
    }
    std::vector<std::uint8_t> pixels(256 * 256 / 2);
    Texture texture{pixels.data(), pixels.size(), 256, 256, 1};
    Material material;
    material.texture = &texture;
    std::array<Triangle, 1> triangles{{{{{{0, 0, 0, 0, 1}, {496, 0, 256, 0, .5f}, {0, 384, 0, 256, 1}}}, &material}}};
    Frame frame{triangles};
    const char *failure = nullptr;
    assert(valid_frame(frame, failure));
    const auto original = triangles[0];
    for (unsigned width : {8u, 16u, 32u, 64u, 128u, 256u})
        for (unsigned height : {8u, 16u, 32u, 64u, 128u, 256u}) {
            texture.width = width; texture.height = height;
            texture.bytes = size_t(width) * height / 2;
            assert(valid_frame(frame, failure));
            --texture.bytes;
            assert(!valid_frame(frame, failure));
            ++texture.bytes;
        }
    texture.width = 7; assert(!valid_frame(frame, failure));
    texture.width = 48; assert(!valid_frame(frame, failure));
    texture.width = 512; assert(!valid_frame(frame, failure));
    texture.width = 256;
    texture.indices = nullptr; assert(!valid_frame(frame, failure)); texture.indices = pixels.data();
    for (float value : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
        triangles[0] = original; triangles[0].vertices[0].x = value; assert(!valid_frame(frame, failure));
        triangles[0] = original; triangles[0].vertices[0].q = value; assert(!valid_frame(frame, failure));
        triangles[0] = original; triangles[0].vertices[0].u = value; assert(!valid_frame(frame, failure));
        triangles[0] = original; triangles[0].vertices[0].depth = value; assert(!valid_frame(frame, failure));
    }
    triangles[0] = original; triangles[0].vertices[0].q = 0; assert(!valid_frame(frame, failure));
    triangles[0] = original; triangles[0].vertices[0].x = 993; assert(!valid_frame(frame, failure));
    triangles[0] = original; triangles[0].vertices[0].y = -385; assert(!valid_frame(frame, failure));
    triangles[0] = original; triangles[0].material = nullptr; assert(!valid_frame(frame, failure));
    triangles[0] = original; material.texture = nullptr; assert(valid_frame(frame, failure));
    frame.depth_test = true;
    triangles[0].vertices[0].depth = -0.001f; assert(!valid_frame(frame, failure));
    triangles[0].vertices[0].depth = 1.001f; assert(!valid_frame(frame, failure));
    triangles[0].vertices[0].depth = 0; assert(valid_frame(frame, failure));
    triangles[0].vertices[0].depth = 1; assert(valid_frame(frame, failure));
    // Exact flat-geometry encodings must still validate every other field.
    // Nonzero/subnormal UV and adjacent Q encodings keep their normal checks.
    for (unsigned corner = 0; corner < 3; ++corner) {
        for (unsigned signs = 0; signs < 4; ++signs) {
            triangles[0] = original;
            auto &vertex = triangles[0].vertices[corner];
            vertex.u = std::bit_cast<float>(std::uint32_t(signs & 1u) << 31);
            vertex.v = std::bit_cast<float>(std::uint32_t((signs >> 1) & 1u) << 31);
            vertex.q = 1;
            assert(valid_frame(frame, failure));
            const auto flat = triangles[0];
            for (unsigned field = 0; field < 3; ++field) {
                triangles[0] = flat;
                auto &v = triangles[0].vertices[corner];
                if (field == 0) v.x = 993;
                if (field == 1) v.y = -385;
                if (field == 2) v.depth = std::bit_cast<float>(std::uint32_t{0x3f800001});
                assert(!valid_frame(frame, failure));
            }
        }
        for (auto bits : {0u, 0x80000000u, 1u, 0x80000001u, 0x49742400u, 0xc9742400u}) {
            triangles[0] = original;
            triangles[0].vertices[corner].u = triangles[0].vertices[corner].v = std::bit_cast<float>(std::uint32_t(bits));
            assert(valid_frame(frame, failure));
        }
        for (auto bits : {0x49742401u, 0xc9742401u, 0x7f800000u, 0xff800000u, 0x7fc00001u}) {
            for (bool use_v : {false, true}) {
                triangles[0] = original;
                auto &v = triangles[0].vertices[corner];
                (use_v ? v.v : v.u) = std::bit_cast<float>(std::uint32_t(bits));
                assert(!valid_frame(frame, failure));
            }
        }
        for (auto bits : {0x00800000u, 0x3f7fffffu, 0x3f800000u, 0x3f800001u, 0x49742400u}) {
            triangles[0] = original; triangles[0].vertices[corner].q = std::bit_cast<float>(std::uint32_t(bits));
            assert(valid_frame(frame, failure));
        }
        for (auto bits : {0u, 0x80000000u, 0xbf800000u, 0x49742401u, 0x7f800000u, 0x7fc00001u}) {
            triangles[0] = original; triangles[0].vertices[corner].q = std::bit_cast<float>(std::uint32_t(bits));
            assert(!valid_frame(frame, failure));
        }
    }
    assert(valid_frame(Frame{}, failure));
    std::vector<Triangle> excessive(16385, original);
    assert(!valid_frame(Frame{excessive}, failure));
    const auto depth=ps2::test::depth_checks(); assert(depth.pass);
    std::printf("NATIVE DEPTH QUANT PASS checks=%u hash=%016llx\n",depth.count,static_cast<unsigned long long>(depth.hash));
}
