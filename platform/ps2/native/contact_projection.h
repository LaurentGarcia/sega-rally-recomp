#pragma once
#include <array>
#include <span>

namespace ps2::native {
struct ContactPointXZ { float x, z; };
struct ContactProjection {
    std::array<float, 4> edge_cross{};
    bool accepted = false;
};

// Original projected edge-sign predicate for exactly three or four vertices.
// Every input binary32 coordinate must be in [-2^20, 2^20], including signed
// zeros and subnormals. Invalid input leaves output unchanged. All input
// coordinates are consumed before the output is committed.
// This does not validate convexity, search course records, calculate height,
// or implement a general polygon/collision test. Signed zero affects acceptance.
bool contact_projection(std::span<const ContactPointXZ> vertices,
                        const ContactPointXZ &point, ContactProjection &output) noexcept;
} // namespace ps2::native
