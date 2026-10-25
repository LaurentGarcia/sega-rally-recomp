#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace ps2::native {
struct ContactVertex { float x, y, z; };
struct ContactPolygon {
    std::array<ContactVertex, 4> vertices{};
    std::array<float, 4> coefficients{};
    std::uint32_t attributes = 0;
};
struct ContactSegment { std::span<const ContactPolygon> polygons; };
struct ContactSearchResult {
    bool matched = false;
    std::uint16_t segment = 0, polygon = 0;
    ContactPolygon data{};
};

// Search current, next, previous; stop at the first projected match. No pointer
// to caller storage survives the call: a hit copies its entire original-format
// geometry value and attributes; a valid miss produces an all-zero payload.
// The course has 1..65535 segments and the signed index must be in range.
// Each visited segment has 1..255 polygons; encountered X/Z coordinates obey
// contact_projection's domain. Y and coefficients are untouched payload bits.
// Unsupported inputs return false and leave output unchanged. No allocation,
// normalization, height selection, result-slot state, or command transport.
[[nodiscard]] bool search_course_contact(std::span<const ContactSegment> course,
                                         std::int16_t current, const ContactVertex &point,
                                         ContactSearchResult &output) noexcept;
} // namespace ps2::native
