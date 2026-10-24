#include "native_display.h"
#include "native_depth_checks.h"
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
    assert(valid_frame(Frame{}, failure));
    std::vector<Triangle> excessive(16385, original);
    assert(!valid_frame(Frame{excessive}, failure));
    const auto depth=ps2::test::depth_checks(); assert(depth.pass);
    std::printf("NATIVE DEPTH QUANT PASS checks=%u hash=%016llx\n",depth.count,static_cast<unsigned long long>(depth.hash));
}
