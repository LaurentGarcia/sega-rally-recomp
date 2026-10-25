#include "rounded_cross_product.h"
#include <array>
#include <bit>
#include <cstdint>
#include <limits>

namespace ps2::native {
namespace {
// Local reuse of the independently written contact-projection dyadic method.
// Keeping this bounded implementation private leaves validated components
// unchanged; it is not a general floating-point processor implementation.
constexpr std::uint32_t sign_bit = 0x80000000u;
struct Dyadic { std::uint32_t numerator; int power; };
Dyadic parts(std::uint32_t bits) noexcept {
    const auto exponent = (bits >> 23) & 255u;
    return {(bits & 0x7fffffu) | (exponent ? 0x800000u : 0u),
            exponent ? int(exponent) - 150 : -149};
}
std::uint32_t rounded(std::uint64_t numerator, int power, std::uint32_t sign) noexcept {
    if (!numerator) return sign;
    const int top = int(std::bit_width(numerator)) - 1;
    const int shift = (top + power < -126) ? -149 - power : top - 23;
    if (shift > 0) {
        if (shift >= 64) return sign; // At most 56 numerator bits enter here.
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
    // The smaller binary32 operand cannot reach half the narrower adjacent
    // spacing once the scale gap exceeds 32. The larger is already rounded.
    if (gap > 32) return left;
    const std::uint64_t first = std::uint64_t(a.numerator) << gap;
    const std::uint64_t second = b.numerator;
    if ((left ^ right) & sign_bit) {
        if (first == second) return 0;
        return first > second ? rounded(first - second, b.power, left & sign_bit)
                              : rounded(second - first, b.power, right & sign_bit);
    }
    return rounded(first + second, b.power, left & sign_bit);
}
std::uint32_t difference(std::uint32_t left, std::uint32_t right) noexcept {
    return sum(left, right ^ sign_bit);
}
using Words = std::array<std::uint32_t, 3>;
Words bits(const CrossVector3 &value) noexcept {
    return {std::bit_cast<std::uint32_t>(value.x), std::bit_cast<std::uint32_t>(value.y),
            std::bit_cast<std::uint32_t>(value.z)};
}
} // namespace

bool rounded_cross_product(const CrossVector3 &left, const CrossVector3 &right,
                           CrossVector3 &output) noexcept {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::radix == 2 &&
                  std::numeric_limits<float>::digits == 24 &&
                  std::bit_cast<std::uint32_t>(1.0f) == 0x3f800000u);
    const auto a = bits(left), b = bits(right);
    for (unsigned i = 0; i < 3; ++i)
        if ((a[i] & 0x7fffffffu) > 0x49800000u || (b[i] & 0x7fffffffu) > 0x49800000u)
            return false;
    Words result;
    for (unsigned i = 0; i < 3; ++i) {
        const auto j = (i + 1) % 3, k = (i + 2) % 3;
        result[i] = difference(product(a[j], b[k]), product(a[k], b[j]));
    }
    output = {std::bit_cast<float>(result[0]), std::bit_cast<float>(result[1]),
              std::bit_cast<float>(result[2])};
    return true;
}
} // namespace ps2::native
