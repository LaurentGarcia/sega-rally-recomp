#include "native_display.h"
#include <bit>
#include <cmath>

namespace ps2::native {
namespace {
bool finite(float value) { return (std::bit_cast<std::uint32_t>(value) & 0x7f800000u) != 0x7f800000u; }
bool dimension(unsigned value) { return value >= 8 && value <= 256 && !(value & (value - 1)); }
}

bool valid_frame(const Frame &frame, const char *&failure) {
    // Reject the complete frame before touching GS state or cache contents.
    if (frame.triangles.size() > 16384) { failure = "NATIVE TRIANGLE LIMIT EXCEEDED"; return false; }
    for (const auto &triangle : frame.triangles) {
        if (!triangle.material) { failure = "NATIVE MATERIAL IS MISSING"; return false; }
        const auto *texture = triangle.material->texture;
        if (texture && (!texture->indices || !dimension(texture->width) || !dimension(texture->height) ||
                        texture->bytes < size_t(texture->width) * texture->height / 2)) {
            failure = "INVALID NATIVE PSMT4 TEXTURE"; return false;
        }
        for (const auto &vertex : triangle.vertices) {
            if (!finite(vertex.x) || !finite(vertex.y) || vertex.x < -496 || vertex.x > 992 ||
                vertex.y < -384 || vertex.y > 768 || !finite(vertex.u) || !finite(vertex.v) ||
                !finite(vertex.q) || !(vertex.q > 0 && vertex.q <= 1000000) ||
                std::fabs(vertex.u) > 1000000 || std::fabs(vertex.v) > 1000000) {
                failure = "INVALID NATIVE VERTEX"; return false;
            }
        }
    }
    failure = "NONE";
    return true;
}

} // namespace ps2::native
