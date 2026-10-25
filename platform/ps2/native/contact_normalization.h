#pragma once

namespace ps2::native {
struct ContactDirection3 { float x = 0, y = 0, z = 0; };

// Recover the contact response's separately rounded length and divisions.
// Inputs must be finite binary32 in [-2^20,2^20]. A rounded squared length
// of zero selects (0,0,1), including underflow of nonzero input squares.
// Signed zeros/subnormals are supported; rejection leaves output unchanged.
// Output may alias input. No host/EE floating arithmetic computes the result.
bool normalize_contact_direction(const ContactDirection3 &input,
                                 ContactDirection3 &output) noexcept;
} // namespace ps2::native
