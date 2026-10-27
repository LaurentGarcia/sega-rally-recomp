#pragma once
#include <cstdint>

namespace ps2::native::vehicle_math {
// Shared value arithmetic for recovered vehicle expressions. There is no
// processor state, instruction dispatch, rounding-mode register or memory bus.
// Nonzero values are normalized to 53 significand bits. Operations round to
// nearest/even after each explicit binary64 boundary. Binary64 subnormal and
// non-finite results are outside this contract and propagate valid=false.
struct Number {
    std::uint64_t significand=0;
    int power=0;
    bool negative=false;
    bool valid=true;
};
Number from_single(float value) noexcept;
Number from_bits(std::uint64_t binary64) noexcept;
Number from_integer(std::int32_t value) noexcept;
Number negate(Number value) noexcept;
Number add(Number left,Number right) noexcept;
Number subtract(Number left,Number right) noexcept;
Number multiply(Number left,Number right) noexcept;
Number divide(Number numerator,Number denominator) noexcept;
// Compare valid values; invalid inputs return false without changing order.
bool compare(Number left,Number right,int& order) noexcept;
bool truncate(Number value,std::int32_t& output) noexcept;
// Narrowing preserves signed zero and binary32 subnormals. An unsupported
// value/overflow returns a quiet NaN sentinel; callers must reject it with
// finite(), never publish it as recovered game state.
float narrow(Number value) noexcept;
bool finite(float value) noexcept;
float add32(float left,float right) noexcept;
float sub32(float left,float right) noexcept;
float mul32(float left,float right) noexcept;
float div32(float numerator,float denominator) noexcept;
float sqrt32(float value) noexcept;
} // namespace ps2::native::vehicle_math
