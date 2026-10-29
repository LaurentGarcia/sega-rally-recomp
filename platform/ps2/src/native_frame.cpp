#include "native_display.h"
#include <bit>
#include <cmath>

namespace ps2::native {
std::uint16_t reverse_depth16(float depth) noexcept {
    // Exact binary32 significand arithmetic avoids EE FPU rounding at a
    // quantization boundary. Half-way cases round upward; input is in[0,1].
    const auto bits = std::bit_cast<std::uint32_t>(depth) & 0x7fffffffu;
    const unsigned exponent = bits >> 23;
    if (exponent < 110) return 0;
    const unsigned shift = 150 - exponent; // Validated input gives23..40.
    const std::uint64_t numerator = std::uint64_t((bits & 0x7fffffu) | 0x800000u) * 65535u;
    return static_cast<std::uint16_t>((numerator + (std::uint64_t{1} << (shift - 1))) >> shift);
}
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
            // Flat native geometry uses exact signed-zero UV and unit Q.
            // Other encodings retain the original checks, including subnormals.
            const bool unit_uv =
                ((std::bit_cast<std::uint32_t>(vertex.u) | std::bit_cast<std::uint32_t>(vertex.v)) & 0x7fffffffu) == 0 &&
                std::bit_cast<std::uint32_t>(vertex.q) == 0x3f800000u;
            if (!finite(vertex.x) || !finite(vertex.y) || vertex.x < -496 || vertex.x > 992 ||
                vertex.y < -384 || vertex.y > 768 ||
                (!unit_uv && (!finite(vertex.u) || !finite(vertex.v) || !finite(vertex.q) ||
                 !(vertex.q > 0 && vertex.q <= 1000000) ||
                 std::fabs(vertex.u) > 1000000 || std::fabs(vertex.v) > 1000000)) ||
                !finite(vertex.depth) || vertex.depth < 0 || vertex.depth > 1) {
                failure = "INVALID NATIVE VERTEX"; return false;
            }
        }
    }
    failure = "NONE";
    return true;
}

} // namespace ps2::native
