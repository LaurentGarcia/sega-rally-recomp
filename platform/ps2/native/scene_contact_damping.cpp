#include "scene_contact_damping.h"
#include "recovered_scalar_metric.h"
#include "vehicle_arithmetic.h"
#include <bit>
#include <cstring>
namespace ps2::native {
bool correct_scene_contact_damping(const SceneContactGeometry& geometry,
                                   float base, float& output) noexcept {
    namespace math = vehicle_math;
    const auto bounded = [](float v) {
        return (std::bit_cast<std::uint32_t>(v) & 0x7fffffffu) <= 0x49800000u;
    };
    const auto point = [&](AffinePoint3 p) { return bounded(p.x) && bounded(p.y) && bounded(p.z); };
    if (!bounded(base) || !point(geometry.published_point) ||
        !point(geometry.reference_point) || !point(geometry.direction)) return false;
    const AffinePoint3 delta{
        math::sub32(geometry.published_point.x, geometry.reference_point.x),
        math::sub32(geometry.published_point.y, geometry.reference_point.y),
        math::sub32(geometry.published_point.z, geometry.reference_point.z)};
    const auto dot = math::add32(
        math::add32(math::mul32(geometry.direction.x, delta.x),
                    math::mul32(geometry.direction.y, delta.y)),
        math::mul32(geometry.direction.z, delta.z));
    if (!math::finite(dot)) return false;
    if (dot <= 0) {
        std::memcpy(&output, &base, sizeof output);
        return true;
    }
    float metric;
    // Preserve the original adjacent FIFO pairs, including the repeated zero.
    // This is neither a Euclidean norm nor a normalization substitution.
    if (!recovered_scalar_metric({0, 0, delta.y}, {0, delta.x, delta.z}, metric)) return false;
    const auto squared = math::mul32(metric, metric);
    if (!math::finite(squared) || (std::bit_cast<std::uint32_t>(squared) & 0x7fffffffu) == 0) return false;
    const auto quotient = math::div32(dot, squared);
    if (!math::finite(quotient)) return false;
    const auto adjusted = math::add(math::from_single(base),
        math::multiply(math::from_single(quotient), math::from_bits(0x3f747ae147ae147bull)));
    int order = 0;
    if (!math::compare(adjusted, math::from_integer(1), order)) return false;
    const auto result = order > 0 ? 1.0f : math::narrow(adjusted);
    if (!math::finite(result)) return false;
    output = result;
    return true;
}
} // namespace ps2::native
