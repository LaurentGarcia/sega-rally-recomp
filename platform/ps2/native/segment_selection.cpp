#include "segment_selection.h"
#include <bit>

namespace ps2::native {
bool select_course_segment(std::span<const ContactPointXZ> vertices,
                           std::span<const CourseSegmentCorners> segments,
                           const ContactPointXZ& point, std::int16_t progression,
                           SegmentSelection& output) noexcept {
    if (segments.empty() || segments.size() > 65535 || vertices.empty() || vertices.size() > 65536)
        return false;
    constexpr std::uint32_t bound = 0x49800000u;
    if ((std::bit_cast<std::uint32_t>(point.x) & 0x7fffffffu) > bound ||
        (std::bit_cast<std::uint32_t>(point.z) & 0x7fffffffu) > bound) return false;
    const auto count = static_cast<std::int32_t>(segments.size());
    auto start = static_cast<std::int32_t>(progression) % count;
    if (start < 0) start += count;
    SegmentSelection result;
    for (std::int32_t visit = 0; visit < count; ++visit) {
        const auto offset = (visit & 1) ? visit / 2 + 1 : -(visit / 2);
        const auto selected = (start + count + offset) % count;
        std::array<ContactPointXZ, 4> corners;
        for (unsigned i = 0; i < 4; ++i) {
            const auto index = segments[static_cast<std::size_t>(selected)][i];
            if (index >= vertices.size()) return false;
            corners[i] = vertices[index];
        }
        ContactProjection projection;
        if (!contact_projection(corners, point, projection)) return false;
        if (!projection.accepted) continue;
        result.selected = selected;
        result.current = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(selected));
        result.progression = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
            static_cast<std::int32_t>(progression) + offset));
        break;
    }
    output = result;
    return true;
}
} // namespace ps2::native
