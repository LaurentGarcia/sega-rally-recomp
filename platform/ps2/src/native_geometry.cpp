#include "native_geometry.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace ps2::native {
namespace {
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::radix == 2 &&
              std::numeric_limits<double>::digits == 53,
              "Reference clipping requires binary64 scratch arithmetic");
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::radix == 2 &&
              std::numeric_limits<float>::digits == 24,
              "Native vertex encoding requires binary32 components");
struct WideVertex { double x, y, z, w, u, v; };
using Polygon = std::array<WideVertex, clip_polygon_capacity>;

bool accepted_float(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value) & 0x7fffffffu;
    return bits == 0 || (bits >= 0x00800000u && bits <= 0x47800000u);
}
bool normal_or_zero(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value) & 0x7fffffffu;
    return bits == 0 || (bits >= 0x00800000u && bits < 0x7f800000u);
}
double distance(const WideVertex &p, unsigned plane) {
    switch (plane) {
    case 0: return p.w - double(clip_min_w);
    case 1: return p.w + p.x;
    case 2: return p.w - p.x;
    case 3: return p.w + p.y;
    case 4: return p.w - p.y;
    case 5: return p.z;
    default: return p.w - p.z;
    }
}
void snap(WideVertex &p, unsigned plane) {
    switch (plane) {
    case 0: p.w = clip_min_w; break;
    case 1: p.x = -p.w; break;
    case 2: p.x = p.w; break;
    case 3: p.y = -p.w; break;
    case 4: p.y = p.w; break;
    case 5: p.z = 0; break;
    default: p.z = p.w; break;
    }
}
bool equal(const WideVertex &a, const WideVertex &b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w && a.u == b.u && a.v == b.v;
}
bool before(const WideVertex &a, const WideVertex &b) {
    const double av[] = {a.x, a.y, a.z, a.w, a.u, a.v};
    const double bv[] = {b.x, b.y, b.z, b.w, b.u, b.v};
    for (unsigned i = 0; i < 6; ++i) {
        if (av[i] < bv[i]) return true;
        if (av[i] > bv[i]) return false;
    }
    return false;
}
bool intersection(WideVertex a, WideVertex b, unsigned plane, WideVertex &p) {
    // An edge traversed in reverse gets identical arithmetic and endpoints.
    if (before(b, a)) std::swap(a, b);
    const double da = distance(a, plane), db = distance(b, plane);
    const double denominator = da - db;
    if (denominator == 0 || !std::isfinite(denominator)) return false;
    const double t = da / denominator;
    if (!(t >= 0 && t <= 1)) return false;
    const double other = 1 - t;
    p = {other*a.x + t*b.x, other*a.y + t*b.y, other*a.z + t*b.z,
         other*a.w + t*b.w, other*a.u + t*b.u, other*a.v + t*b.v};
    snap(p, plane);
    return true;
}
bool append(Polygon &polygon, std::size_t &count, const WideVertex &p) {
    if (count && equal(polygon[count - 1], p)) return true;
    if (count == polygon.size()) return false;
    polygon[count++] = p;
    return true;
}
bool repair_roundoff(WideVertex &p) {
    // Inputs have already been strictly clipped. At most seven interpolation
    // stages precede this bounded repair; a larger violation fails closed.
    const double scale = std::max({std::fabs(p.x), std::fabs(p.y), std::fabs(p.z),
                                   std::fabs(p.w), double(clip_min_w)});
    const double tolerance = 64 * std::numeric_limits<double>::epsilon() * scale;
    for (unsigned plane = 0; plane < 7; ++plane) {
        const double d = distance(p, plane);
        if (!std::isfinite(d) || d < -tolerance) return false;
        if (d < 0) snap(p, plane);
    }
    return true;
}
bool project(WideVertex p, Vertex &v) {
    if (!repair_roundoff(p)) return false;
    v = {float((p.x / p.w + 1) * (frame_width / 2.0)),
         float((1 - p.y / p.w) * (frame_height / 2.0)),
         float(p.u), float(p.v), float(1 / p.w)};
    return normal_or_zero(v.x) && normal_or_zero(v.y) && normal_or_zero(v.u) &&
           normal_or_zero(v.v) && normal_or_zero(v.q) &&
           v.x >= 0 && v.x <= frame_width && v.y >= 0 && v.y <= frame_height &&
           v.q >= 0x1p-16f && v.q <= 0x1p10f &&
           std::fabs(v.u) <= clip_component_limit && std::fabs(v.v) <= clip_component_limit;
}
bool nonzero_area(const std::array<Vertex, 3> &v) {
    return (double(v[1].x) - v[0].x) * (double(v[2].y) - v[0].y) !=
           (double(v[1].y) - v[0].y) * (double(v[2].x) - v[0].x);
}
} // namespace

ClipResult clip_project_triangle(const ClipTriangle &input, std::span<Triangle> output) noexcept {
    if (!input.material) return {ClipStatus::invalid_material, 0, 0};
    Polygon first{}, second{};
    for (std::size_t i = 0; i < 3; ++i) {
        const auto &v = input.vertices[i];
        if (!accepted_float(v.x) || !accepted_float(v.y) || !accepted_float(v.z) ||
            !accepted_float(v.w) || !accepted_float(v.u) || !accepted_float(v.v))
            return {ClipStatus::invalid_vertex, 0, 0};
        first[i] = {v.x, v.y, v.z, v.w, v.u, v.v};
    }
    auto *from = &first, *to = &second;
    std::size_t count = 3;
    for (unsigned plane = 0; plane < 7 && count; ++plane) {
        std::size_t next_count = 0;
        WideVertex previous = (*from)[count - 1];
        bool previous_inside = distance(previous, plane) >= 0;
        for (std::size_t i = 0; i < count; ++i) {
            const auto current = (*from)[i];
            const bool current_inside = distance(current, plane) >= 0;
            if (current_inside != previous_inside) {
                WideVertex crossing{};
                if (!intersection(previous, current, plane, crossing) || !append(*to, next_count, crossing))
                    return {ClipStatus::numerical_failure, 0, 0};
            }
            if (current_inside && !append(*to, next_count, current))
                return {ClipStatus::numerical_failure, 0, 0};
            previous = current; previous_inside = current_inside;
        }
        if (next_count > 1 && equal((*to)[0], (*to)[next_count - 1])) --next_count;
        count = next_count;
        std::swap(from, to);
    }
    if (count < 3) return {};
    std::array<Vertex, clip_polygon_capacity> projected{};
    for (std::size_t i = 0; i < count; ++i)
        if (!project((*from)[i], projected[i])) return {ClipStatus::numerical_failure, 0, 0};
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
