#include "native_geometry_float.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace ps2::native {
namespace {
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::radix == 2 &&
              std::numeric_limits<float>::digits == 24);
using Polygon = std::array<ClipVertex, clip_polygon_capacity>;

bool accepted(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value) & 0x7fffffffu;
    return bits == 0 || (bits >= 0x00800000u && bits <= 0x47800000u);
}
bool normal_or_zero(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value) & 0x7fffffffu;
    return bits == 0 || (bits >= 0x00800000u && bits < 0x7f800000u);
}
float distance(const ClipVertex &p, unsigned plane) {
    switch (plane) {
    case 0: return p.w - clip_min_w;
    case 1: return p.w + p.x;
    case 2: return p.w - p.x;
    case 3: return p.w + p.y;
    case 4: return p.w - p.y;
    case 5: return p.z;
    default: return p.w - p.z;
    }
}
void snap(ClipVertex &p, unsigned plane) {
    switch (plane) {
    case 0: p.w = clip_min_w; break;
    case 1: p.x = -p.w; break;
    case 2: p.x = p.w; break;
    case 3: p.y = -p.w; break;
    case 4: p.y = p.w; break;
    case 5: p.z = 0.0f; break;
    default: p.z = p.w; break;
    }
}
bool same(const ClipVertex &a, const ClipVertex &b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w &&
           a.u == b.u && a.v == b.v;
}
bool before(const ClipVertex &a, const ClipVertex &b) {
    const float av[] = {a.x, a.y, a.z, a.w, a.u, a.v};
    const float bv[] = {b.x, b.y, b.z, b.w, b.u, b.v};
    for (unsigned i = 0; i < 6; ++i) {
        if (av[i] < bv[i]) return true;
        if (av[i] > bv[i]) return false;
    }
    return false;
}
bool intersection(ClipVertex a, ClipVertex b, unsigned plane, ClipVertex &p) {
    // Adjacent faces traversing the same edge in reverse use the same order.
    if (before(b, a)) std::swap(a, b);
    const float da = distance(a, plane), db = distance(b, plane);
    const float denominator = da - db;
    if (!normal_or_zero(denominator) || denominator == 0.0f) return false;
    const float t = da / denominator;
    if (!(t >= 0.0f && t <= 1.0f)) return false;
    const float other = 1.0f - t;
    p = {other*a.x + t*b.x, other*a.y + t*b.y, other*a.z + t*b.z,
         other*a.w + t*b.w, other*a.u + t*b.u, other*a.v + t*b.v};
    snap(p, plane);
    return normal_or_zero(p.x) && normal_or_zero(p.y) && normal_or_zero(p.z) &&
           normal_or_zero(p.w) && normal_or_zero(p.u) && normal_or_zero(p.v);
}
bool append(Polygon &polygon, std::size_t &count, const ClipVertex &p) {
    if (count && same(polygon[count - 1], p)) return true;
    if (count == polygon.size()) return false;
    polygon[count++] = p;
    return true;
}
bool repair_roundoff(ClipVertex &p) {
    const float scale = std::max({std::fabs(p.x), std::fabs(p.y), std::fabs(p.z),
                                  std::fabs(p.w), clip_min_w});
    const float tolerance = 0x1p-19f * scale; // 16 binary32 epsilons.
    for (unsigned plane = 0; plane < 7; ++plane) {
        const float d = distance(p, plane);
        if (!normal_or_zero(d) || d < -tolerance) return false;
        if (d < 0.0f) snap(p, plane);
    }
    return true;
}
bool project(ClipVertex p, Vertex &v, bool original_inside) {
    if (!original_inside && !repair_roundoff(p)) return false;
    v = {(p.x / p.w + 1.0f) * (float(frame_width) * 0.5f),
         (1.0f - p.y / p.w) * (float(frame_height) * 0.5f),
         p.u, p.v, 1.0f / p.w, 1.0f - p.z / p.w};
    return normal_or_zero(v.x) && normal_or_zero(v.y) && normal_or_zero(v.u) &&
           normal_or_zero(v.v) && normal_or_zero(v.q) && normal_or_zero(v.depth) &&
           v.x >= 0.0f && v.x <= float(frame_width) &&
           v.y >= 0.0f && v.y <= float(frame_height) &&
           v.depth >= 0.0f && v.depth <= 1.0f &&
           v.q >= 0x1p-16f && v.q <= 0x1p10f &&
           std::fabs(v.u) <= clip_component_limit && std::fabs(v.v) <= clip_component_limit;
}
bool nonzero_area(const std::array<Vertex, 3> &v) {
    return (v[1].x - v[0].x) * (v[2].y - v[0].y) !=
           (v[1].y - v[0].y) * (v[2].x - v[0].x);
}
} // namespace

ClipResult clip_project_triangle_float(const ClipTriangle &input,
                                       std::span<Triangle> output) noexcept {
    if (!input.material) return {ClipStatus::invalid_material, 0, 0};
    unsigned any_outside = 0, common_outside = 0x7f;
    for (const auto &v : input.vertices) {
        if (!accepted(v.x) || !accepted(v.y) || !accepted(v.z) ||
            !accepted(v.w) || !accepted(v.u) || !accepted(v.v))
            return {ClipStatus::invalid_vertex, 0, 0};
        const unsigned outside = (v.w < clip_min_w ? 1u : 0u) |
            (v.x < -v.w ? 2u : 0u) | (v.x > v.w ? 4u : 0u) |
            (v.y < -v.w ? 8u : 0u) | (v.y > v.w ? 16u : 0u) |
            (v.z < 0.0f ? 32u : 0u) | (v.z > v.w ? 64u : 0u);
        any_outside |= outside;
        common_outside &= outside;
    }
    if (common_outside) return {};
    Polygon first{}, second{};
    std::copy(input.vertices.begin(), input.vertices.end(), first.begin());
    auto *from = &first, *to = &second;
    std::size_t count = 3;
    for (unsigned plane = 0; any_outside && plane < 7 && count; ++plane) {
        std::size_t next = 0;
        ClipVertex previous = (*from)[count - 1];
        bool previous_inside = distance(previous, plane) >= 0.0f;
        for (std::size_t i = 0; i < count; ++i) {
            const auto current = (*from)[i];
            const bool current_inside = distance(current, plane) >= 0.0f;
            if (current_inside != previous_inside) {
                ClipVertex crossing{};
                if (!intersection(previous, current, plane, crossing) || !append(*to, next, crossing))
                    return {ClipStatus::numerical_failure, 0, 0};
            }
            if (current_inside && !append(*to, next, current))
                return {ClipStatus::numerical_failure, 0, 0};
            previous = current;
            previous_inside = current_inside;
        }
        if (next > 1 && same((*to)[0], (*to)[next - 1])) --next;
        count = next;
        std::swap(from, to);
    }
    if (count < 3) return {};
    std::array<Vertex, clip_polygon_capacity> projected{};
    for (std::size_t i = 0; i < count; ++i)
        if (!project((*from)[i], projected[i], any_outside == 0))
            return {ClipStatus::numerical_failure, 0, 0};
    std::array<Triangle, clip_triangle_capacity> staged{};
    std::size_t triangles = 0;
    for (std::size_t i = 1; i + 1 < count; ++i) {
        const std::array<Vertex, 3> fan{projected[0], projected[i], projected[i + 1]};
        if (nonzero_area(fan)) staged[triangles++] = {fan, input.material};
    }
    if (output.size() < triangles) return {ClipStatus::insufficient_capacity, 0, triangles};
    std::copy_n(staged.begin(), triangles, output.begin());
    return {ClipStatus::success, triangles, triangles};
}
} // namespace ps2::native
