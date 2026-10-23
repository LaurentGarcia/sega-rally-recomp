#include "native_display.h"
#include <cassert>
#include <limits>
#include <vector>

int main() {
    using namespace ps2::native;
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
    }
    triangles[0] = original; triangles[0].vertices[0].q = 0; assert(!valid_frame(frame, failure));
    triangles[0] = original; triangles[0].vertices[0].x = 993; assert(!valid_frame(frame, failure));
    triangles[0] = original; triangles[0].vertices[0].y = -385; assert(!valid_frame(frame, failure));
    triangles[0] = original; triangles[0].material = nullptr; assert(!valid_frame(frame, failure));
    triangles[0] = original; material.texture = nullptr; assert(valid_frame(frame, failure));
    assert(valid_frame(Frame{}, failure));
    std::vector<Triangle> excessive(16385, original);
    assert(!valid_frame(Frame{excessive}, failure));
}
