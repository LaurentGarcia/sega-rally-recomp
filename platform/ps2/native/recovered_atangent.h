#pragma once

namespace ps2::native {
// Bounded scalar atan(argument), corresponding to the player's atan2(x,1).
// Accepts finite binary32 values, preserves signed zero and rejects nonfinite
// input atomically. Project-authored range reduction and rounded arithmetic;
// no host transcendental, CPU registers or instruction runtime is involved.
// Original-boundary and numerical comparisons establish the tested domain;
// this is not a correctly-rounded transcendental library for arbitrary inputs.
[[nodiscard]] bool recovered_atangent(float argument,float& output) noexcept;
} // namespace ps2::native
