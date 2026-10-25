#include "cabinet_steering.h"
#include <bit>
#include <limits>

namespace ps2::native {

float cabinet_steering(std::uint8_t sample) noexcept {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::digits == 24 &&
                  std::bit_cast<std::uint32_t>(1.0f) == 0x3f800000u);
    const int centered = int(sample) - 128;
    unsigned magnitude = unsigned(centered < 0 ? -centered : centered);
    if (magnitude > 80) magnitude = 80;
    if (!magnitude) return std::bit_cast<float>(std::uint32_t(0));

    // For all 81 possible magnitudes, rounding the exact rational 21*m/2000
    // agrees with the original two binary64 operations followed by binary32.
    // Exhaustive original-program comparison establishes this bounded identity.
    const unsigned numerator = 21 * magnitude;
    int exponent = -1;
    while ((numerator << unsigned(-exponent)) < 2000u) --exponent;
    const std::uint64_t scaled = std::uint64_t(numerator) << unsigned(23 - exponent);
    auto significand = std::uint32_t(scaled / 2000u);
    const auto remainder = unsigned(scaled % 2000u);
    if (remainder > 1000u || (remainder == 1000u && (significand & 1))) ++significand;
    if (significand == 0x1000000u) { significand >>= 1; ++exponent; }
    const auto sign = centered < 0 ? 0x80000000u : 0u;
    return std::bit_cast<float>(sign | (std::uint32_t(exponent + 127) << 23) |
                               (significand & 0x7fffffu));
}

} // namespace ps2::native
