#include "contact_projection.h"
#include <bit>
#include <cstdint>
#include <limits>

namespace ps2::native {
namespace {
// Bounded exact binary32 operations, independently written for native game
// arithmetic. Integer rounding preserves the original semantics on the EE.
constexpr std::uint32_t sign_bit = 0x80000000u, magnitude_mask = 0x7fffffffu;
struct Dyadic { std::uint32_t numerator; int power; };

Dyadic parts(std::uint32_t bits) noexcept {
    const auto exponent = (bits >> 23) & 255u;
    return {(bits & 0x7fffffu) | (exponent ? 0x800000u : 0u),
            exponent ? int(exponent) - 150 : -149};
}

// Round an exact unsigned dyadic to binary32, retaining signed underflow zero.
// Only bounded finite results enter this private arithmetic: no infinities,
// exceptions, processor registers or general instruction semantics are modeled.
std::uint32_t rounded(std::uint64_t numerator, int power, std::uint32_t sign) noexcept {
    if (!numerator) return sign;
    const int top = int(std::bit_width(numerator)) - 1;
    const int shift = (top + power < -126) ? -149 - power : top - 23;
    if (shift > 0) {
        if (shift >= 64) return sign; // Numerators here have at most 56 bits.
        const auto discarded = numerator & ((std::uint64_t(1) << shift) - 1);
        const auto half = std::uint64_t(1) << (shift - 1);
        numerator >>= shift;
        if (discarded > half || (discarded == half && (numerator & 1))) ++numerator;
    } else {
        numerator <<= -shift;
    }
    power += shift;
    if (numerator >= 0x1000000u) { numerator >>= 1; ++power; }
    if (numerator < 0x800000u) return sign | std::uint32_t(numerator);
    return sign | (std::uint32_t(power + 150) << 23) | (std::uint32_t(numerator) & 0x7fffffu);
}

std::uint32_t product(std::uint32_t left, std::uint32_t right) noexcept {
    const auto a = parts(left), b = parts(right);
    return rounded(std::uint64_t(a.numerator) * b.numerator, a.power + b.power,
                   (left ^ right) & sign_bit);
}

std::uint32_t sum(std::uint32_t left, std::uint32_t right) noexcept {
    auto a = parts(left), b = parts(right);
    if (!a.numerator && !b.numerator) return (left & right) & sign_bit;
    if (!a.numerator) return right;
    if (!b.numerator) return left;
    if (a.power < b.power) {
        const auto saved = left; left = right; right = saved;
        const auto value = a; a = b; b = value;
    }
    const unsigned gap = unsigned(a.power - b.power);
    // Both operands are already binary32. Beyond 32 exponent steps, the smaller
    // cannot reach even half the narrower neighboring spacing of the larger.
    if (gap > 32) return left;
    const std::uint64_t first = std::uint64_t(a.numerator) << gap;
    const std::uint64_t second = b.numerator;
    if ((left ^ right) & sign_bit) {
        if (first == second) return 0; // Exact cancellation at nearest/even.
        return first > second ? rounded(first - second,b.power,left & sign_bit)
                              : rounded(second - first,b.power,right & sign_bit);
    }
    return rounded(first + second,b.power,left & sign_bit);
}


std::uint32_t difference(std::uint32_t left, std::uint32_t right) noexcept {
    return sum(left, right ^ sign_bit);
}
bool bounded(std::uint32_t value) noexcept {
    return (value & magnitude_mask) <= 0x49800000u; // 2^20.
}
} // namespace

bool contact_projection(std::span<const ContactPointXZ> vertices,
                        const ContactPointXZ &point, ContactProjection &output) noexcept {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::radix == 2 &&
                  std::numeric_limits<float>::digits == 24 &&
                  std::bit_cast<std::uint32_t>(1.0f) == 0x3f800000u);
    if (vertices.size() != 3 && vertices.size() != 4) return false;
    const auto x = std::bit_cast<std::uint32_t>(point.x);
    const auto z = std::bit_cast<std::uint32_t>(point.z);
    if (!bounded(x) || !bounded(z)) return false;
    std::array<std::uint32_t, 4> dx{}, dz{};
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const auto vx = std::bit_cast<std::uint32_t>(vertices[i].x);
        const auto vz = std::bit_cast<std::uint32_t>(vertices[i].z);
        if (!bounded(vx) || !bounded(vz)) return false;
        dx[i] = difference(vx, x);
        dz[i] = difference(vz, z);
    }
    ContactProjection result;
    std::uint32_t any_sign = 0, every_sign = sign_bit;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const auto next = (i + 1) % vertices.size();
        const auto cross = difference(product(dx[i], dz[next]), product(dx[next], dz[i]));
        result.edge_cross[i] = std::bit_cast<float>(cross);
        any_sign |= cross & sign_bit;
        every_sign &= cross & sign_bit;
    }
    // The original checks sign bits, not cross >= 0. A zero edge may therefore
    // reject one winding while accepting its reverse; do not add an epsilon.
    result.accepted = !any_sign || every_sign;
    output = result;
    return true;
}
} // namespace ps2::native
