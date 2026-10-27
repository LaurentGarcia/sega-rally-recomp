// Original platform geometry fixtures. No game data or reference runtime.
#include "native_geometry.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#ifndef RALLY_GEOMETRY_EXACT_ONLY
#include <vector>
#endif
#ifdef _EE
#include <kernel.h>
#endif

namespace {
using namespace ps2::native;
unsigned checks = 0, random_cases = 0;
std::uint64_t fingerprint = 1469598103934665603ull;
Material material;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::printf("NATIVE GEOMETRY FAIL line=%u check=%s\n", unsigned(__LINE__), #condition); return false; } } while (0)

ClipTriangle inside() {
    return {{{{-.5f, -.5f, .5f, 1, 0, 0}, {.5f, -.5f, .5f, 1, 1, 0},
               {0, .5f, .5f, 1, .5f, 1}}}, &material};
}
double area(const Triangle &triangle) {
    const auto &v = triangle.vertices;
    return ((double(v[1].x) - v[0].x) * (double(v[2].y) - v[0].y) -
            (double(v[1].y) - v[0].y) * (double(v[2].x) - v[0].x)) / 2;
}
bool same(const Vertex &a, const Vertex &b) {
    return std::bit_cast<std::uint32_t>(a.x) == std::bit_cast<std::uint32_t>(b.x) &&
           std::bit_cast<std::uint32_t>(a.y) == std::bit_cast<std::uint32_t>(b.y) &&
           std::bit_cast<std::uint32_t>(a.u) == std::bit_cast<std::uint32_t>(b.u) &&
           std::bit_cast<std::uint32_t>(a.v) == std::bit_cast<std::uint32_t>(b.v) &&
           std::bit_cast<std::uint32_t>(a.q) == std::bit_cast<std::uint32_t>(b.q);
}
void hash(const Vertex &v) {
    const float fields[] = {v.x, v.y, v.u, v.v, v.q};
    for (float field : fields) fingerprint = (fingerprint ^ std::bit_cast<std::uint32_t>(field)) * 1099511628211ull;
}
bool valid(const std::array<Triangle, clip_triangle_capacity> &output, std::size_t count) {
    CHECK(count <= clip_triangle_capacity);
    for (std::size_t i = 0; i < count; ++i) {
        CHECK(output[i].material == &material);
        CHECK(area(output[i]) != 0);
        for (const auto &v : output[i].vertices) {
            CHECK(v.x >= 0 && v.x <= 496 && v.y >= 0 && v.y <= 384);
            CHECK(v.q >= 0x1p-16f && v.q <= 1024);
            CHECK(std::isfinite(v.u) && std::isfinite(v.v));
            CHECK(std::isfinite(v.depth) && v.depth >= 0 && v.depth <= 1);
        }
    }
    return true;
}

bool exact_cases() {
    std::array<Triangle, clip_triangle_capacity> output{};
    auto triangle = inside();
    auto result = clip_project_triangle(triangle, output);
    CHECK(result.status == ClipStatus::success && result.count == 1 && result.required == 1);
    const Vertex expected[] = {{124, 288, 0, 0, 1}, {372, 288, 1, 0, 1}, {248, 96, .5f, 1, 1}};
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(same(output[0].vertices[i], expected[i])); hash(output[0].vertices[i]);
        CHECK(output[0].vertices[i].depth == .5f);
    }
    CHECK(area(output[0]) < 0); // y-up clip space becomes y-down screen space.
    std::swap(triangle.vertices[1], triangle.vertices[2]);
    result = clip_project_triangle(triangle, output);
    CHECK(result.count == 1 && area(output[0]) > 0); // No backface culling.

    // Every plane is crossed independently; all endpoints are exact dyadics.
    for (unsigned plane = 0; plane < 7; ++plane) {
        triangle = inside();
        auto &a = triangle.vertices[0];
        switch (plane) {
        case 0:
            triangle = {{{{-0x1p-12f, -0x1p-12f, 0x1p-13f, 0x1p-11f, 0, 0},
                          {0x1p-11f, -0x1p-11f, 0x1p-10f, 0x1p-9f, 1, 0},
                          {0, 0x1p-11f, 0x1p-10f, 0x1p-9f, .5f, 1}}}, &material}; break;
        case 1: a.x = -2; break;
        case 2: a.x = 2; break;
        case 3: a.y = -2; break;
        case 4: a.y = 2; break;
        case 5: a.z = -.5f; break;
        case 6: a.z = 1.5f; break;
        }
        result = clip_project_triangle(triangle, output);
        CHECK(result.status == ClipStatus::success && result.count == 2);
        CHECK(valid(output, result.count));
        for (std::size_t i = 0; i < result.count; ++i)
            for (const auto &v : output[i].vertices) hash(v);
        triangle = inside();
        for (auto &v : triangle.vertices) switch (plane) {
        case 0: v.w = -1; break;
        case 1: v.x -= 3; break;
        case 2: v.x += 3; break;
        case 3: v.y -= 3; break;
        case 4: v.y += 3; break;
        case 5: v.z = -1; break;
        case 6: v.z = 2; break;
        }
        const auto before = output;
        result = clip_project_triangle(triangle, output);
        CHECK(result.status == ClipStatus::success && result.count == 0 && result.required == 0);
        CHECK(std::memcmp(output.data(), before.data(), sizeof output) == 0);
    }

    // A corner on left/top/near and the two other exact viewport corners.
    triangle = {{{{-1, 1, 0, 1, 0, 0}, {1, 1, 1, 1, 8, 0}, {1, -1, 1, 1, 8, 8}}}, &material};
    result = clip_project_triangle(triangle, output);
    CHECK(result.status == ClipStatus::success && result.count == 1);
    const Vertex corners[] = {{0, 0, 0, 0, 1}, {496, 0, 8, 0, 1}, {496, 384, 8, 8, 1}};
    for (unsigned i = 0; i < 3; ++i) CHECK(same(output[0].vertices[i], corners[i]));
    CHECK(output[0].vertices[0].depth == 1 && output[0].vertices[1].depth == 0 && output[0].vertices[2].depth == 0);

    // On-plane endpoints next to outside vertices must not produce duplicate
    // corner fans. Two outside vertices leave only one boundary point.
    triangle = {{{{-1, 0, .5f, 1, 0, 0}, {-2, -.5f, .5f, 1, 1, 0},
                   {0, .75f, .5f, 1, 0, 1}}}, &material};
    result = clip_project_triangle(triangle, output);
    CHECK(result.status == ClipStatus::success && result.count == 1);
    triangle.vertices[2].x = -2;
    result = clip_project_triangle(triangle, {});
    CHECK(result.status == ClipStatus::success && result.count == 0 && result.required == 0);

    // Near intersections occur at t=1/3. Raw UV must be 2, not an interpolation
    // of u/w or already projected UV. Interpolated w=4/3, so q rounds to .75.
    triangle = {{{{-1, 0, -1, 1, 0, 0}, {2, 0, 2, 2, 6, 0}, {0, 2, 2, 2, 0, 6}}}, &material};
    result = clip_project_triangle(triangle, output);
    CHECK(result.status == ClipStatus::success && result.count == 2);
    bool first = false, second = false;
    for (std::size_t i = 0; i < result.count; ++i) for (const auto &v : output[i].vertices) {
        if (same(v, {248, 192, 2, 0, .75f})) first = true;
        if (same(v, {124, 96, 0, 2, .75f})) second = true;
    }
    CHECK(first && second);

    // Duplicate vertices and collinear projected vertices produce no fans.
    for (unsigned mode = 0; mode < 3; ++mode) {
        triangle = inside();
        if (mode == 0) { triangle.vertices[1] = triangle.vertices[0]; triangle.vertices[1].u = 11; }
        else if (mode == 1) for (auto &v : triangle.vertices) v.y = 0;
        else for (auto &v : triangle.vertices) v.w = 0;
        const auto before = output;
        result = clip_project_triangle(triangle, output);
        CHECK(result.status == ClipStatus::success && result.count == 0);
        CHECK(std::memcmp(output.data(), before.data(), sizeof output) == 0);
    }
    for (float w : {clip_min_w, clip_component_limit}) {
        triangle = inside();
        for (auto &v : triangle.vertices) { v.x *= w; v.y *= w; v.z *= w; v.w = w; }
        result = clip_project_triangle(triangle, output);
        CHECK(result.status == ClipStatus::success && result.count == 1);
        CHECK(output[0].vertices[0].q == 1 / w);
    }
    return true;
}

bool failure_cases() {
    std::array<Triangle, clip_triangle_capacity + 2> output;
    std::memset(output.data(), 0x5a, sizeof output);
    const auto before = output;
    const auto unchanged = [&] { return std::memcmp(output.data(), before.data(), sizeof output) == 0; };
    auto triangle = inside();
    const float invalid[] = {std::bit_cast<float>(0x7f800000u), std::bit_cast<float>(0xff800000u),
        std::bit_cast<float>(0x7fc12345u), std::bit_cast<float>(0x7f812345u),
        std::bit_cast<float>(0x00000001u), std::bit_cast<float>(0x807fffffu),
        std::bit_cast<float>(0x47800001u), -std::bit_cast<float>(0x47800001u)};
    for (unsigned component = 0; component < 6; ++component) for (float value : invalid) {
        triangle = inside();
        auto &v = triangle.vertices[2];
        float *fields[] = {&v.x, &v.y, &v.z, &v.w, &v.u, &v.v};
        *fields[component] = value;
        const auto r = clip_project_triangle(triangle, {output.data() + 1, clip_triangle_capacity});
        CHECK(r.status == ClipStatus::invalid_vertex && r.count == 0 && r.required == 0);
        CHECK(unchanged());
    }
    // Geometric rejection cannot hide an invalid attribute in a later
    // vertex. Exercise every trivial outside plane with untouched output.
    for (unsigned plane = 0; plane < 7; ++plane) {
        triangle = inside();
        for (auto &v : triangle.vertices) switch (plane) {
        case 0: v.w = -1; break;
        case 1: v.x = -2; break;
        case 2: v.x = 2; break;
        case 3: v.y = -2; break;
        case 4: v.y = 2; break;
        case 5: v.z = -1; break;
        default: v.z = 2; break;
        }
        triangle.vertices[2].u = std::bit_cast<float>(0x7fc12345u);
        const auto rejected = clip_project_triangle(triangle, output);
        CHECK(rejected.status == ClipStatus::invalid_vertex && rejected.count == 0 && rejected.required == 0);
        CHECK(unchanged());
    }
    triangle = inside(); triangle.material = nullptr;
    CHECK(clip_project_triangle(triangle, output).status == ClipStatus::invalid_material);
    CHECK(unchanged());
    triangle = inside();
    auto result = clip_project_triangle(triangle, {});
    CHECK(result.status == ClipStatus::insufficient_capacity && result.count == 0 && result.required == 1);
    triangle.vertices[0].z = -.5f;
    result = clip_project_triangle(triangle, {output.data() + 1, 1});
    CHECK(result.status == ClipStatus::insufficient_capacity && result.count == 0 && result.required == 2);
    CHECK(unchanged());
    result = clip_project_triangle(triangle, {output.data() + 1, 2});
    CHECK(result.status == ClipStatus::success && result.count == 2 && result.required == 2);
    CHECK(std::memcmp(&output.front(), &before.front(), sizeof(Triangle)) == 0);
    CHECK(std::memcmp(output.data() + 3, before.data() + 3, sizeof(Triangle) * (output.size() - 3)) == 0);

    // A minimum-normal UV is valid input, but a near-plane interpolation to
    // half that value is subnormal output. A late failure must publish nothing.
    std::memcpy(output.data(), before.data(), sizeof output);
    triangle = inside();
    triangle.vertices[0].z = -.5f;
    triangle.vertices[0].u = std::bit_cast<float>(0x00800000u);
    triangle.vertices[1].u = triangle.vertices[2].u = 0;
    result = clip_project_triangle(triangle, {output.data() + 1, clip_triangle_capacity});
    CHECK(result.status == ClipStatus::numerical_failure && result.count == 0 && result.required == 0);
    CHECK(unchanged());

    // Signed zero is accepted, including in texture attributes. Input bounds
    // are bit-tested; they do not rely on EE comparisons with NaN encodings.
    triangle = inside(); triangle.vertices[2].u = -0.f;
    CHECK(clip_project_triangle(triangle, output).status == ClipStatus::success);

    // Opposite traversal of a shared, non-dyadic near intersection must give
    // exactly the same projected attributes. No epsilon-based vertex welding.
    triangle = {{{{-.75f, -.25f, -.5f, 1.5f, 7, 3}, {.5f, .25f, .75f, 2, 9, 5},
                   {0, .75f, .5f, 1, 0, 0}}}, &material};
    std::array<Triangle, clip_triangle_capacity> a{}, b{};
    const auto ra = clip_project_triangle(triangle, a);
    std::swap(triangle.vertices[0], triangle.vertices[1]);
    triangle.vertices[2] = {-.5f, -.75f, .5f, 1, 0, 0};
    const auto rb = clip_project_triangle(triangle, b);
    CHECK(ra.status == ClipStatus::success && rb.status == ClipStatus::success);
    bool found = false;
    for (std::size_t i = 0; i < ra.count; ++i) for (const auto &av : a[i].vertices)
        if (std::fabs(av.u - 7.8f) < .00001f)
            for (std::size_t j = 0; j < rb.count; ++j) for (const auto &bv : b[j].vertices)
                if (same(av, bv)) found = true;
    CHECK(found);
    return true;
}

#ifndef RALLY_GEOMETRY_EXACT_ONLY
// Independent exact rational oracle. In barycentric (s,t), every clipping
// constraint is an integer half-plane. Enumerate all pairs of boundary lines,
// retain their exact feasible intersections, then order the resulting polygon.
// No production clipping/interpolation loop is reused. Random input components
// are multiples of 1/4 in [-8,8]; 1024-scaled determinants/products fit int64.
struct Constraint { std::int64_t a, b, c; };
struct RationalPoint { std::int64_t s, t, denominator; };
struct OracleVertex { double x, y, u, v, q, depth; };
std::vector<OracleVertex> oracle(const ClipTriangle &triangle) {
    std::array<Constraint, 10> planes{{{1, 0, 0}, {0, 1, 0}, {-1, -1, 1}}};
    std::int64_t distances[3][7]{};
    for (unsigned i = 0; i < 3; ++i) {
        const auto &v = triangle.vertices[i];
        const auto x = std::int64_t(v.x * 1024), y = std::int64_t(v.y * 1024);
        const auto z = std::int64_t(v.z * 1024), w = std::int64_t(v.w * 1024);
        const std::int64_t d[] = {w - 1, w + x, w - x, w + y, w - y, z, w - z};
        std::copy_n(d, 7, distances[i]);
    }
    for (unsigned p = 0; p < 7; ++p)
        planes[p + 3] = {distances[1][p] - distances[0][p],
                         distances[2][p] - distances[0][p], distances[0][p]};
    std::vector<RationalPoint> feasible;
    for (unsigned i = 0; i < planes.size(); ++i) for (unsigned j = i + 1; j < planes.size(); ++j) {
        const auto &a = planes[i], &b = planes[j];
        auto det = a.a * b.b - b.a * a.b;
        if (!det) continue;
        auto s = a.b * b.c - b.b * a.c, t = a.c * b.a - b.c * a.a;
        if (det < 0) { det = -det; s = -s; t = -t; }
        bool accepted = true;
        for (const auto &plane : planes)
            if (plane.a * s + plane.b * t + plane.c * det < 0) accepted = false;
        if (!accepted) continue;
        bool duplicate = false;
        for (const auto &p : feasible)
            if (p.s * det == s * p.denominator && p.t * det == t * p.denominator) duplicate = true;
        if (!duplicate) feasible.push_back({s, t, det});
    }
    double center_s = 0, center_t = 0;
    for (const auto &p : feasible) { center_s += double(p.s) / p.denominator; center_t += double(p.t) / p.denominator; }
    if (!feasible.empty()) { center_s /= feasible.size(); center_t /= feasible.size(); }
    std::sort(feasible.begin(), feasible.end(), [&](const auto &a, const auto &b) {
        return std::atan2(double(a.t) / a.denominator - center_t, double(a.s) / a.denominator - center_s) <
               std::atan2(double(b.t) / b.denominator - center_t, double(b.s) / b.denominator - center_s);
    });
    std::vector<OracleVertex> result;
    for (const auto &p : feasible) {
        const double s = double(p.s) / p.denominator, t = double(p.t) / p.denominator;
        const auto &a = triangle.vertices[0], &b = triangle.vertices[1], &c = triangle.vertices[2];
        const double x = a.x + (b.x - double(a.x)) * s + (c.x - double(a.x)) * t;
        const double y = a.y + (b.y - double(a.y)) * s + (c.y - double(a.y)) * t;
        const double z = a.z + (b.z - double(a.z)) * s + (c.z - double(a.z)) * t;
        const double w = a.w + (b.w - double(a.w)) * s + (c.w - double(a.w)) * t;
        result.push_back({(x / w + 1) * 248, (1 - y / w) * 192,
            a.u + (b.u - double(a.u)) * s + (c.u - double(a.u)) * t,
            a.v + (b.v - double(a.v)) * s + (c.v - double(a.v)) * t, 1 / w, 1-z/w});
    }
    return result;
}
bool randomized_oracle() {
    std::uint32_t random = 0x710fad19u;
    const auto number = [&]() {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        return float(int(random % 65) - 32) / 4;
    };
    std::array<Triangle, clip_triangle_capacity> output{};
    std::size_t maximum_fan = 0;
    for (unsigned iteration = 0; iteration < 20000; ++iteration) {
        ClipTriangle triangle; triangle.material = &material;
        for (auto &v : triangle.vertices) v = {number(), number(), number(), number(), number(), number()};
        const auto reference = oracle(triangle);
        const auto result = clip_project_triangle(triangle, output);
        if (result.status != ClipStatus::success)
            std::printf("random iteration=%u status=%u\n", iteration, unsigned(result.status));
        CHECK(result.status == ClipStatus::success);
        CHECK(valid(output, result.count));
        maximum_fan = std::max(maximum_fan, result.count);
        double expected_area = 0, actual_area = 0;
        for (std::size_t i = 0; i < reference.size(); ++i) {
            const auto &a = reference[i], &b = reference[(i + 1) % reference.size()];
            expected_area += a.x * b.y - a.y * b.x;
        }
        expected_area = std::fabs(expected_area) / 2;
        for (std::size_t i = 0; i < result.count; ++i) {
            actual_area += std::fabs(area(output[i]));
            for (const auto &v : output[i].vertices) {
                bool match = false;
                for (const auto &r : reference)
                    if (std::fabs(v.x - r.x) < .0001 && std::fabs(v.y - r.y) < .0001 &&
                        std::fabs(v.u - r.u) < .00001 && std::fabs(v.v - r.v) < .00001 &&
                        std::fabs(v.q - r.q) < std::max(.00001, std::fabs(r.q) * .000001) &&
                        std::fabs(v.depth-r.depth) < .00001) match = true;
                CHECK(match);
            }
        }
        CHECK(std::fabs(actual_area - expected_area) < std::max(.05, expected_area * .000002));
        ++random_cases;
    }
    CHECK(maximum_fan >= 5); // Exercise multi-plane growth, not just near quads.
    std::printf("NATIVE GEOMETRY oracle cases=%u max_fan=%u\n", random_cases, unsigned(maximum_fan));
    return true;
}
#endif
} // namespace

int main() {
    bool passed = exact_cases() && failure_cases();
    std::printf("NATIVE GEOMETRY exact_hash=%08lx%08lx\n", (unsigned long)(fingerprint >> 32),
                (unsigned long)(fingerprint & 0xffffffffu));
#ifndef RALLY_GEOMETRY_EXACT_ONLY
    if (passed) passed = randomized_oracle();
#endif
    std::printf("NATIVE GEOMETRY %s checks=%u random=%u\n", passed ? "PASS" : "FAIL", checks, random_cases);
    std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return passed ? 0 : 1;
}
