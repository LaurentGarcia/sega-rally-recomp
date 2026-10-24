// Typed index selection recovered from the contact-query preparation boundary.
// This does not calculate intersections, heights, normals, or contact forces.
#pragma once

#include <cstdint>

namespace ps2::native {

struct ContactNeighborhood {
    std::uint16_t previous = 0;
    std::uint16_t current = 0;
    std::uint16_t next = 0;
    bool operator==(const ContactNeighborhood &) const = default;
};

// Asset-policy limit, not a claim about every original command/count encoding.
inline constexpr std::uint32_t max_contact_segments = 65535;

// Select the ordered cyclic neighbors of a valid signed car segment index.
// Counts must be 1..max_contact_segments; negative/out-of-range indices and
// unsupported counts return false and leave output unchanged. No allocation,
// address-space access, floating-point arithmetic, or device commands occur.
[[nodiscard]] bool contact_neighborhood(std::uint32_t segment_count,
                                       std::int16_t current,
                                       ContactNeighborhood &output) noexcept;

} // namespace ps2::native
