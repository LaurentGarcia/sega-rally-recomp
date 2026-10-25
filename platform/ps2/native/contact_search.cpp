#include "contact_search.h"
#include "contact_neighborhood.h"
#include "contact_projection.h"
#include <cstring>
#include <type_traits>

namespace ps2::native {
bool search_course_contact(std::span<const ContactSegment> course,
                           std::int16_t current, const ContactVertex &point,
                           ContactSearchResult &output) noexcept {
    if (course.size() > max_contact_segments) return false;
    ContactNeighborhood neighborhood;
    if (!contact_neighborhood(static_cast<std::uint32_t>(course.size()), current, neighborhood)) return false;
    const ContactPointXZ projected{point.x, point.z};
    for (const auto segment : {neighborhood.current, neighborhood.next, neighborhood.previous}) {
        const auto polygons = course[segment].polygons;
        if (polygons.empty() || polygons.size() > 255) return false;
        for (std::size_t i = 0; i < polygons.size(); ++i) {
            const auto &polygon = polygons[i];
            const std::size_t edges = (polygon.attributes & 4u) ? 4 : 3;
            std::array<ContactPointXZ, 4> vertices;
            for (std::size_t j = 0; j < edges; ++j)
                vertices[j] = {polygon.vertices[j].x, polygon.vertices[j].z};
            ContactProjection projection;
            if (!contact_projection(std::span(vertices.data(), edges), projected, projection)) return false;
            if (projection.accepted) {
                ContactSearchResult result;
                result.matched = true;
                result.segment = segment;
                result.polygon = static_cast<std::uint16_t>(i);
                // Payloads may include arbitrary Y/coefficient encodings. Copy
                // their object representation without floating arithmetic.
                static_assert(std::is_trivially_copyable_v<ContactPolygon>);
                std::memcpy(&result.data, &polygon, sizeof polygon);
                std::memcpy(&output, &result, sizeof result);
                return true;
            }
        }
    }
    const ContactSearchResult miss{};
    std::memcpy(&output, &miss, sizeof miss);
    return true;
}
} // namespace ps2::native
