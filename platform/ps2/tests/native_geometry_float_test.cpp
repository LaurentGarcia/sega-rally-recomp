// Original diagnostic geometry only. Reference arithmetic is test-only.
#include "native_geometry_float.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
using namespace ps2::native;
Material material;
unsigned checks = 0, compared = 0, nonempty = 0;
float max_pixels = 0, max_depth = 0, max_uv = 0, max_relative_q = 0;
using Output = std::array<Triangle, clip_triangle_capacity>;
#define CHECK(c) do { ++checks; if (!(c)) { std::printf("FLOAT GEOMETRY FAIL line=%u %s\n", unsigned(__LINE__), #c); return false; } } while (0)
ClipTriangle inside() {
    return {{{{-.5f, -.5f, .5f, 1, 0, 0}, {.5f, -.5f, .5f, 1, 1, 0},
               {0, .5f, .5f, 1, .5f, 1}}}, &material};
}
std::array<unsigned char, sizeof(Output)> snapshot(const Output &out) {
    std::array<unsigned char, sizeof(Output)> bytes{};
    std::memcpy(bytes.data(), out.data(), bytes.size());
    return bytes;
}
bool unchanged(const Output &out, const std::array<unsigned char, sizeof(Output)> &bytes) {
    return std::memcmp(out.data(), bytes.data(), bytes.size()) == 0;
}
bool valid(const Output &out, std::size_t count) {
    CHECK(count <= clip_triangle_capacity);
    for (std::size_t i = 0; i < count; ++i) {
        CHECK(out[i].material == &material);
        for (const auto &v : out[i].vertices) {
            const float components[] = {v.x, v.y, v.u, v.v, v.q, v.depth};
            for (float f : components) {
                const auto bits = std::bit_cast<std::uint32_t>(f) & 0x7fffffffu;
                CHECK(bits == 0 || (bits >= 0x00800000u && bits < 0x7f800000u));
            }
            CHECK(v.x >= 0 && v.x <= frame_width && v.y >= 0 && v.y <= frame_height);
            CHECK(v.depth >= 0 && v.depth <= 1 && v.q >= 0x1p-16f && v.q <= 1024);
            CHECK(std::fabs(v.u) <= clip_component_limit && std::fabs(v.v) <= clip_component_limit);
        }
    }
    return true;
}
bool compare(const ClipTriangle &triangle) {
    Output ref{}, actual{};
    const auto a = clip_project_triangle(triangle, ref);
    const auto b = clip_project_triangle_float(triangle, actual);
    CHECK(a.status == ClipStatus::success);
    CHECK(b.status == ClipStatus::success);
    CHECK(a.count == b.count && b.required == b.count);
    CHECK(valid(actual, b.count));
    ++compared;
    nonempty += b.count != 0;
    for (std::size_t i = 0; i < b.count; ++i) for (unsigned j = 0; j < 3; ++j) {
        const auto &x = ref[i].vertices[j]; const auto &y = actual[i].vertices[j];
        max_pixels = std::max({max_pixels, std::fabs(x.x-y.x), std::fabs(x.y-y.y)});
        max_depth = std::max(max_depth, std::fabs(x.depth-y.depth));
        max_uv = std::max({max_uv, std::fabs(x.u-y.u), std::fabs(x.v-y.v)});
        max_relative_q = std::max(max_relative_q, std::fabs(x.q-y.q)/x.q);
        CHECK(std::fabs(x.x-y.x) <= 0.125f && std::fabs(x.y-y.y) <= 0.125f);
        CHECK(std::fabs(x.depth-y.depth) <= 0x1p-14f);
        CHECK(std::fabs(x.u-y.u) <= 0.125f && std::fabs(x.v-y.v) <= 0.125f);
        CHECK(std::fabs(x.q-y.q)/x.q <= 0x1p-10f);
    }
    return true;
}
bool authored() {
    Output out{};
    auto triangle = inside();
    auto r = clip_project_triangle_float(triangle, out);
    CHECK(r.status == ClipStatus::success && r.count == 1);
    CHECK(out[0].vertices[0].x == 124 && out[0].vertices[0].y == 288);
    CHECK(out[0].vertices[1].x == 372 && out[0].vertices[2].y == 96);
    CHECK(compare(triangle));
    // The inside-only path has the same atomic capacity contract as clipping.
    const auto inside_bytes = snapshot(out);
    r = clip_project_triangle_float(triangle, std::span<Triangle>(out).first(0));
    CHECK(r.status == ClipStatus::insufficient_capacity && r.count == 0 && r.required == 1);
    CHECK(unchanged(out, inside_bytes));
    auto flat_inside = triangle;
    flat_inside.vertices[2].y = flat_inside.vertices[0].y;
    r = clip_project_triangle_float(flat_inside, std::span<Triangle>(out).first(0));
    CHECK(r.status == ClipStatus::success && r.count == 0 && r.required == 0);
    CHECK(unchanged(out, inside_bytes));
    triangle.vertices[2].u = -0.0f;
    out[1].vertices[0].x = 123.0f;
    const auto tail_bytes = snapshot(out);
    r = clip_project_triangle_float(triangle, std::span<Triangle>(out).first(1));
    CHECK(r.status == ClipStatus::success && r.count == 1 && r.required == 1);
    CHECK(std::bit_cast<std::uint32_t>(out[0].vertices[2].u) == 0x80000000u);
    CHECK(std::memcmp(out.data() + 1, tail_bytes.data() + sizeof(Triangle),
                      sizeof(out) - sizeof(Triangle)) == 0);
    for (unsigned plane = 0; plane < 7; ++plane) {
        triangle = inside();
        auto &a = triangle.vertices[0];
        switch (plane) {
        case 0: triangle = {{{{-0x1p-12f,-0x1p-12f,0x1p-13f,0x1p-11f,0,0},
                              {0x1p-11f,-0x1p-11f,0x1p-10f,0x1p-9f,1,0},
                              {0,0x1p-11f,0x1p-10f,0x1p-9f,.5f,1}}}, &material}; break;
        case 1: a.x = -2; break; case 2: a.x = 2; break;
        case 3: a.y = -2; break; case 4: a.y = 2; break;
        case 5: a.z = -.5f; break; default: a.z = 1.5f; break;
        }
        CHECK(compare(triangle));
        r = clip_project_triangle_float(triangle, out);
        CHECK(r.count == 2);
        const auto bytes = snapshot(out);
        r = clip_project_triangle_float(triangle, std::span<Triangle>(out).first(1));
        CHECK(r.status == ClipStatus::insufficient_capacity && r.count == 0 && r.required == 2);
        CHECK(unchanged(out, bytes));
        for (auto &v : triangle.vertices) switch (plane) {
        case 0: v.w = -1; break; case 1: v.x = -4; break; case 2: v.x = 4; break;
        case 3: v.y = -4; break; case 4: v.y = 4; break;
        case 5: v.z = -2; break; default: v.z = 4; break;
        }
        r = clip_project_triangle_float(triangle, out);
        CHECK(r.status == ClipStatus::success && r.count == 0 && unchanged(out, bytes));
    }
    // UV interpolation is in clip space, before reciprocal w. At the left
    // crossing t=1/3: raw u=4 and w=4/3, therefore q=3/4.
    triangle = {{{{-2,0,.5f,1,2,0},{0,-1,1,2,8,0},{0,1,1,2,8,1}}}, &material};
    CHECK(compare(triangle));
    r = clip_project_triangle_float(triangle, out);
    unsigned crossings = 0;
    for (std::size_t i = 0; i < r.count; ++i) for (const auto &v : out[i].vertices)
        if (v.x == 0) { ++crossings; CHECK(std::fabs(v.u-4) < 0x1p-18f && std::fabs(v.q-.75f) < 0x1p-20f); }
    CHECK(crossings >= 2);
    // A vertex exactly on the plane must not create a duplicate fan.
    triangle = inside(); triangle.vertices[0].z = 0; triangle.vertices[1].z = -1;
    CHECK(compare(triangle));
    // Coincident positions with different UVs and a collinear fan are empty.
    triangle = inside(); triangle.vertices[2] = triangle.vertices[0]; triangle.vertices[2].u = 9;
    r = clip_project_triangle_float(triangle, out); CHECK(r.status == ClipStatus::success && r.count == 0);
    triangle = inside(); triangle.vertices[2].x = 0; triangle.vertices[2].y = -.5f;
    r = clip_project_triangle_float(triangle, out); CHECK(r.status == ClipStatus::success && r.count == 0);
    // Behind-eye and extreme cancellation cases must fail atomically or
    // produce bounded geometry; no accuracy promise is made for these inputs.
    const std::array<ClipTriangle, 3> difficult{{
        {{{{-1,-1,-1,-1,0,0},{1,-1,1,2,1,0},{0,1,1,2,0,1}}}, &material},
        {{{{-65536,-65536,0,clip_min_w,0,0},{65536,65536,0,clip_min_w,1,0},{0,1,1,2,0,1}}}, &material},
        {{{{-1,-1,0,1,0,0},{1,1,1,1,1,0},{0,0,0,1,0,1}}}, &material}
    }};
    for (const auto &d : difficult) {
        const auto bytes = snapshot(out); r = clip_project_triangle_float(d, out);
        CHECK(r.status == ClipStatus::success || r.status == ClipStatus::numerical_failure);
        if (r.status == ClipStatus::success) CHECK(valid(out, r.count)); else CHECK(unchanged(out, bytes));
    }
    return true;
}
bool failures() {
    Output out{}; CHECK(clip_project_triangle_float(inside(), out).count == 1);
    const auto bytes = snapshot(out);
    auto triangle = inside(); triangle.material = nullptr;
    CHECK(clip_project_triangle_float(triangle, out).status == ClipStatus::invalid_material);
    CHECK(unchanged(out, bytes));
    for (std::uint32_t raw : {1u,0x007fffffu,0x7f800000u,0xff800000u,0x7fc00001u,0x47800001u}) {
        for (unsigned component = 0; component < 6; ++component) {
            triangle = inside(); auto &v = triangle.vertices[2];
            float *fields[] = {&v.x,&v.y,&v.z,&v.w,&v.u,&v.v};
            *fields[component] = std::bit_cast<float>(raw);
            CHECK(clip_project_triangle_float(triangle, out).status == ClipStatus::invalid_vertex);
            CHECK(unchanged(out, bytes));
        }
    }
    triangle = inside(); triangle.vertices[0].z = -.5f;
    triangle.vertices[0].u = std::bit_cast<float>(0x00800000u);
    triangle.vertices[1].u = triangle.vertices[2].u = 0;
    CHECK(clip_project_triangle_float(triangle, out).status == ClipStatus::numerical_failure);
    CHECK(unchanged(out, bytes));
    return true;
}
std::uint32_t seed = 0x6a09e667u;
std::uint32_t random_word() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
bool randomized() {
    // Positive w spans six powers of two within each triangle. Dyadic clip
    // and texel inputs cover all side/z planes while avoiding intentionally
    // ill-conditioned eye-plane divisions in this quantitative corpus.
    for (unsigned i = 0; i < 20000; ++i) {
        ClipTriangle t{}; t.material = &material;
        const int exponent = int(random_word()%13)-6;
        for (auto &v : t.vertices) {
            v.w = std::ldexp(1.0f + float(random_word()%1024)/1024.0f,
                             exponent + int(random_word()%6));
            auto signed_unit = [] { return float(int(random_word()%4097)-2048)/1024.0f; };
            v.x = signed_unit()*v.w; v.y = signed_unit()*v.w;
            v.z = (float(int(random_word()%2049)-512)/1024.0f)*v.w;
            v.u = float(random_word()%4097)/16.0f; v.v = float(random_word()%4097)/16.0f;
        }
        CHECK(compare(t));
    }
    CHECK(nonempty > 10000);
    return true;
}
} // namespace
int main() {
    if (!authored() || !failures() || !randomized()) return 1;
    std::printf("FLOAT GEOMETRY PASS checks=%u compared=%u nonempty=%u max_pixels=%.9g max_depth=%.9g max_uv=%.9g max_relative_q=%.9g\n",
                checks, compared, nonempty, double(max_pixels), double(max_depth), double(max_uv), double(max_relative_q));
    return 0;
}
