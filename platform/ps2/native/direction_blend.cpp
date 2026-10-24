#include "direction_blend.h"
#include <bit>
#include <cstdint>
#include <limits>

namespace ps2::native {
namespace {
constexpr std::uint32_t sign_bit = 0x80000000u, magnitude_mask = 0x7fffffffu;
constexpr std::uint32_t one = 0x3f800000u;
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
        if (shift >= 64) return sign; // Numerators here have at most56 bits.
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
    // Both operands are already binary32. Beyond32 exponent steps, the smaller
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

std::uint32_t component(std::uint32_t base, std::uint32_t target, std::uint32_t weight) noexcept {
    return sum(base, product(sum(target,base ^ sign_bit),weight));
}
} // namespace

bool blend_planar_direction(const PlanarPair &base, const PlanarPair &target,
                            PlanarPair &output) noexcept {
    // EE has this storage layout in our domain, but different FPU arithmetic.
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::radix == 2 &&
                  std::numeric_limits<float>::digits == 24 &&
                  std::numeric_limits<float>::min_exponent == -125 &&
                  std::bit_cast<std::uint32_t>(1.0f) == one);
    const auto a = std::bit_cast<std::uint32_t>(base.first);
    const auto b = std::bit_cast<std::uint32_t>(base.third);
    const auto c = std::bit_cast<std::uint32_t>(target.first);
    const auto d = std::bit_cast<std::uint32_t>(target.third);
    if ((a & magnitude_mask) > one || (b & magnitude_mask) > one ||
        (c & magnitude_mask) > one || (d & magnitude_mask) > one) return false;

    const auto dot = sum(product(a,c),product(b,d));
    // Original weight: min(1, round64(1 - dot)), then round32. For positive
    // binary32 dot in [2^-25,2], subtraction is exact in binary64. Smaller
    // positive dot rounds the weight to1; dot<=0 selects the upper cap1.
    const auto weight = (dot & sign_bit) || !(dot & magnitude_mask) ? one : sum(one,dot ^ sign_bit);
    const PlanarPair result{std::bit_cast<float>(component(a,c,weight)),
                            std::bit_cast<float>(component(b,d,weight))};
    output = result;
    return true;
}
} // namespace ps2::native
