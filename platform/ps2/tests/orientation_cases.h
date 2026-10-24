#pragma once
// Original mathematical boundary vectors. No game data or original programs.
#include <array>
#include <cstdint>

namespace orientation_cases {
using Bits = std::array<std::uint32_t, 3>;
template<class Visit> bool accepted(Visit visit) {
    constexpr std::uint32_t magnitudes[] = {
        0, 1, 0x7fffff, 0x800000, 0x3ec90fd9, 0x3ec90fda,
        0x3ec90fdb, 0x3ec90fdc, 0x3f800000, 0x40490fd9,
        0x40490fda, 0x40490fdb, 0x40490fdc, 0x40c90fda, 0x40c90fdb
    };
    std::array<std::uint32_t, 30> boundaries{};
    for (unsigned i = 0; i < 15; ++i) {
        boundaries[2*i] = magnitudes[i];
        boundaries[2*i+1] = magnitudes[i] | 0x80000000u;
    }
    for (auto a : boundaries) for (auto b : boundaries) for (auto c : boundaries)
        if (!visit(Bits{a,b,c})) return false;
    // Every exponent represented in the accepted range, with sparse fractions.
    for (unsigned exponent = 0; exponent <= 129; ++exponent)
        for (auto fraction : {0u,1u,0x155555u,0x400000u,0x7ffffeu,0x7fffffu}) {
            const auto value = (exponent << 23) | fraction;
            if (value > 0x40c90fdbu) continue;
            if (!visit(Bits{value,value|0x80000000u,value}) ||
                !visit(Bits{value|0x80000000u,value,value|0x80000000u})) return false;
        }
    // Adjacent bit patterns around each mathematical decision boundary.
    for (auto boundary : {0x3ec90fdau,0x40490fdau,0x40c90fdbu})
        for (int displacement = -256; displacement <= 256; ++displacement) {
            const auto value = std::uint32_t(std::int64_t(boundary) + displacement);
            if (value > 0x40c90fdbu) continue;
            if (!visit(Bits{value,value|0x80000000u,value}) ||
                !visit(Bits{value|0x80000000u,value,value|0x80000000u})) return false;
        }
    std::uint32_t random = 0x317ac942;
    auto next = [&] {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        return (random & 0x80000000u) | ((random & 0x7fffffffu) % 0x40c90fdcu);
    };
    for (unsigned i = 0; i < 131072; ++i)
        if (!visit(Bits{next(),next(),next()})) return false;
    // Sample the nontrivial folded interval uniformly, both input signs.
    for (std::uint32_t value = 0x40490fdb; value <= 0x40c90fdb; value += 127)
        if (!visit(Bits{value,value|0x80000000u,value}) ||
            !visit(Bits{value|0x80000000u,value,value|0x80000000u})) return false;
    return true;
}
} // namespace orientation_cases
