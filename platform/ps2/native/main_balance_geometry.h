#pragma once
#include "rounded_affine_transform.h"
#include <array>
#include <cstdint>

namespace ps2::native {
struct MainBalanceGeometryInput {
    // First/second follow the two original groups; no physical axes or units.
    std::array<float,2> longitudinal{}, lateral{}, balance{};
    float steering=0, middle_orientation=0;
    std::uint32_t selected_index=0;
};
struct MainBalanceGeometryState {
    AffinePoint3 first{}, second{};
    std::uint32_t published_index=0;
};
// Complete bounded geometry/publication tail of the main balance helper.
// Longitudinal/lateral magnitudes<=2^16, balances<=16, angles<=8, and initial
// state components<=2^20; all finite, including signed zeros/subnormals.
// Preserves recovered polynomial/transform and separate binary64 boundaries.
// Failure leaves all state untouched. No allocations, mapped state or assets.
[[nodiscard]] bool publish_main_balance_geometry(const MainBalanceGeometryInput& input,
                                                MainBalanceGeometryState& state) noexcept;
} // namespace ps2::native
