#pragma once
#include <array>

namespace ps2::native {
struct PairedBalance {
    std::array<float,2> sums{};
    std::array<float,2> balances{};
};
// Each consecutive pair produces a separately rounded sum and signed balance.
// Inputs are finite binary32 in [-2^20,2^20], including subnormals and signed
// zeros. A zero rounded sum gives positive-zero balance. Otherwise the balance
// retains the original binary64(1.1) product/division rounding boundaries.
// No epsilon, physical units, mapped state or parameter lookup is implied.
// Rejection preserves every output field; inputs are never modified.
[[nodiscard]] bool make_paired_balance(const std::array<float,4>& values,
                                     PairedBalance& output) noexcept;
} // namespace ps2::native
