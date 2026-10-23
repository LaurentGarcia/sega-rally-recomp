#include "angle_fold.h"
#include <bit>
#include <cstdint>
#include <limits>

namespace ps2::native {

bool fold_angle_once(float &angle) noexcept {
    // EE shares the binary32 layout here, but has different FPU behavior and
    // an extra finite exponent. Only the bounded storage layout is required.
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::radix == 2 &&
                  std::numeric_limits<float>::digits == 24 &&
                  std::numeric_limits<float>::min_exponent == -125 &&
                  std::bit_cast<std::uint32_t>(1.0f) == 0x3f800000 &&
                  std::bit_cast<std::uint32_t>(-1.0f) == 0xbf800000);
    constexpr auto half_turn_floor = std::bit_cast<std::uint32_t>(0x1.921fb4p+1f);
    constexpr auto full_turn_nearest = std::bit_cast<std::uint32_t>(0x1.921fb6p+2f);
    const auto input = std::bit_cast<std::uint32_t>(angle);
    const auto magnitude = input & 0x7fffffffu;
    if (magnitude > full_turn_nearest) return false;
    if (magnitude <= half_turn_floor) return true;

    // In the remaining interval, binary32 exponents are 1 or 2. Express
    // both operands in units of 2^-50: the binary64 value 2*pi has this exact
    // integer numerator, and every accepted input is exactly aligned to it.
    constexpr std::uint64_t turn = 0x1921fb54442d18ull;
    const unsigned scale = (magnitude >> 23) - 127 + 27;
    const auto numerator = std::uint64_t((magnitude & 0x7fffff) | 0x800000) << scale;
    const bool negative = numerator < turn;
    const auto distance = negative ? turn - numerator : numerator - turn;

    // The exact difference has 28..52 significant bit positions, so its
    // binary64 rounding is exact. Only the final binary32 rounding remains.
    const unsigned width = std::bit_width(distance);
    const unsigned discarded = width - 24;
    auto significand = distance >> discarded;
    const auto remainder = distance & ((std::uint64_t(1) << discarded) - 1);
    const auto halfway = std::uint64_t(1) << (discarded - 1);
    if (remainder > halfway || (remainder == halfway && (significand & 1))) ++significand;
    unsigned exponent = int(width) - 1 - 50 + 127;
    if (significand == 0x1000000) {
        significand >>= 1;
        ++exponent;
    }
    const auto sign = (input & 0x80000000u) ^ (negative ? 0x80000000u : 0u);
    angle = std::bit_cast<float>(sign | (exponent << 23) |
                                (std::uint32_t(significand) & 0x7fffff));
    return true;
}

} // namespace ps2::native
