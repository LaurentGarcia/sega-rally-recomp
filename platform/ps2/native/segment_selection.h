#pragma once
#include "contact_projection.h"
#include <array>
#include <cstdint>
#include <span>

namespace ps2::native {
using CourseSegmentCorners = std::array<std::uint16_t, 4>;
struct SegmentSelection {
    std::int32_t selected = -1;
    std::int16_t current = -1;
    std::int16_t progression = -1;
};

// Select the first accepted quadrilateral in the recovered alternating search.
// Explicit asset policy: 1..65535 segments and 1..65536 vertices. Only vertices
// referenced by visited segments are consumed; each used X/Z and the point must
// be finite binary32 in [-2^20,2^20]. Y and unused record words have no role.
// Every int16 progression is supported. The previous current index is not read.
// selected is the full nonnegative index (or -1); current preserves its low16
// bits as signed16. progression adds the signed search offset modulo65536.
// A valid search with no match succeeds with all three outputs -1. Invalid
// input fails atomically. Asset/point storage must remain valid throughout the
// call and must not overlap output storage.
bool select_course_segment(std::span<const ContactPointXZ> vertices,
                           std::span<const CourseSegmentCorners> segments,
                           const ContactPointXZ& point, std::int16_t progression,
                           SegmentSelection& output) noexcept;
} // namespace ps2::native
