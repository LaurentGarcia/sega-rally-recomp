#pragma once
#include "contact_result.h"

namespace ps2::native {
struct ContactResponseInput {
    ContactResult contact;
    ContactVertex offset;
    float parameter50 = 1;
    float parameter54 = 1;
};
struct ContactResponseOutput {
    ContactVertex response;
    float selected_scalar = 0;
    bool active = false;
};
// Status bit7 clear gives a positive-zero response and preserves the previous
// scalar's raw encoding; the other inputs are unused. The active branch uses
// finite direction/offset magnitudes <=16, gap/previous magnitudes <=1024,
// positive parameters in [2^-10,2^16], and a nonnegative rounded dot product.
// Those are explicit supported bounds, not inferred physical units.
// Direction input and scratch storage are separate native values, matching
// ordinary main-pass calls. The earlier raw-pointer alias path is not exposed.
// All results are committed together; rejection leaves output unchanged.
[[nodiscard]] bool make_contact_response(const ContactResponseInput& input,
    float previous_scalar, ContactResponseOutput& output) noexcept;
} // namespace ps2::native
