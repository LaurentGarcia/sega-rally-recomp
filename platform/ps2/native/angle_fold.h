#pragma once

namespace ps2::native {

// One turn correction, with binary32 round-to-nearest, ties-to-even results.
// Accepts finite angles from -round32(2*pi) through +round32(2*pi). Values
// outside that explicit domain are rejected without modifying the argument.
// Signed zero and all values already inside [-pi, pi] retain their exact bits.
bool fold_angle_once(float &angle) noexcept;

} // namespace ps2::native
