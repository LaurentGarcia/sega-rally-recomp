#include "orientation.h"
#include "angle_fold.h"
#include <bit>
#include <cstdint>

namespace ps2::native {
namespace {
float bound_outer_angle(float angle) noexcept {
    // The strict binary64 pi/8 comparison lies between these adjacent
    // binary32 values. Preserve untouched bits, including negative zero.
    constexpr auto below_bound = std::bit_cast<std::uint32_t>(0x1.921fb4p-2f);
    constexpr auto rounded_bound = std::bit_cast<std::uint32_t>(0x1.921fb6p-2f);
    const auto bits = std::bit_cast<std::uint32_t>(angle);
    if ((bits & 0x7fffffffu) <= below_bound) return angle;
    return std::bit_cast<float>((bits & 0x80000000u) | rounded_bound);
}
} // namespace

bool finalize_orientation(OrientationAngles &angles) noexcept {
    auto result = angles;
    if (!fold_angle_once(result.first) || !fold_angle_once(result.middle) ||
        !fold_angle_once(result.third)) return false;
    result.first = bound_outer_angle(result.first);
    result.third = bound_outer_angle(result.third);
    angles = result;
    return true;
}

} // namespace ps2::native
