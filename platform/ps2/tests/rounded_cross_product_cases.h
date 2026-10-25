#pragma once
#include "rounded_cross_product.h"
#include <array>
#include <bit>
#include <cstdint>

namespace rounded_cross_product_cases {
using Case = std::array<std::uint32_t, 6>;
using Words = std::array<std::uint32_t, 3>;
inline ps2::native::CrossVector3 vector(const Case &c, unsigned offset) {
    return {std::bit_cast<float>(c[offset]), std::bit_cast<float>(c[offset + 1]),
            std::bit_cast<float>(c[offset + 2])};
}
inline Words bits(const ps2::native::CrossVector3 &v) {
    return {std::bit_cast<std::uint32_t>(v.x), std::bit_cast<std::uint32_t>(v.y),
            std::bit_cast<std::uint32_t>(v.z)};
}
inline std::uint32_t next(std::uint32_t &state) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state;
}
template<class Test> bool each(Test test, bool extended) {
    constexpr std::array<Words, 8> axes{{{0,0,0}, {0x80000000u,0x80000000u,0x80000000u},
        {0x3f800000u,0,0}, {0xbf800000u,0,0}, {0,0x3f800000u,0},
        {0,0xbf800000u,0}, {0,0,0x3f800000u}, {0,0,0xbf800000u}}};
    for (const auto &a : axes) for (const auto &b : axes)
        if (!test(Case{a[0],a[1],a[2],b[0],b[1],b[2]})) return false;
    for (unsigned mask = 0; mask < 64; ++mask) {
        Case c{};
        for (unsigned i = 0; i < 6; ++i) c[i] = ((mask >> i) & 1u) << 31;
        if (!test(c)) return false;
    }
    constexpr std::array<std::uint32_t, 24> boundaries{
        0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x807fffffu,0x00800000u,0x80800000u,
        0x1a000000u,0x1a000001u,0x1a800000u,0x9a000000u,0x3f000000u,0xbf000000u,
        0x3f7ffffeu,0x3f7fffffu,0x3f800000u,0x3f800001u,0xbf800000u,0xbf800001u,
        0x497fffffu,0x49800000u,0xc97fffffu,0xc9800000u};
    for (auto a : boundaries) for (auto b : boundaries)
        for (unsigned rotation = 0; rotation < 3; ++rotation) {
            Case c{};
            c[rotation] = a; c[(rotation+1)%3] = b; c[(rotation+2)%3] = a ^ 0x80000000u;
            c[3+rotation] = b; c[3+(rotation+1)%3] = a; c[3+(rotation+2)%3] = b ^ 0x80000000u;
            if (!test(c)) return false;
        }
    // Products around one, with cancellation after individual product rounding.
    for (unsigned i = 0; i < (extended ? 8192u : 256u); ++i) {
        Case c{0x3f800000u+i,0x3f800001u,0x3f7ffffeu,0x3f800000u,0x3f800000u+i,0x3f800000u};
        if (!test(c)) return false;
    }
    std::uint32_t random = 0x7a0ed391u;
    for (unsigned i = 0; i < (extended ? 100000u : 4096u); ++i) {
        Case c;
        for (auto &v : c) { const auto x = next(random); v=(x & 0x80000000u) | ((x & 0x7fffffffu) % 0x49800001u); }
        if (i % 5 == 0) for (unsigned j = 0; j < 3; ++j) c[3+j] = c[j];
        if (i % 7 == 0) { c[1] = 0; c[4] = 0; }
        if (!test(c)) return false;
    }
    return true;
}
inline void hash(std::uint64_t &value, const Case &input, const Words &output) {
    for (auto word : input) value = (value ^ word) * 1099511628211ull;
    for (auto word : output) value = (value ^ word) * 1099511628211ull;
}
} // namespace rounded_cross_product_cases
