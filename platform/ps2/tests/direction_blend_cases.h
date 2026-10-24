#pragma once
#include <array>
#include <cstdint>

// Original mathematical inputs, independent of any game data or capture.
namespace direction_blend_cases {
using Bits = std::array<std::uint32_t,4>;
inline constexpr std::array<std::uint32_t,22> boundary{
    0,0x80000000u,1,0x80000001u,0x007fffffu,0x807fffffu,0x00800000u,0x80800000u,
    0x3f800000u,0xbf800000u,0x3f7fffffu,0xbf7fffffu,0x3f000000u,0xbf000000u,
    0x3effffffu,0x3f000001u,0x3f400000u,0xbf400000u,0x33000000u,0x32ffffffu,
    0x33000001u,0xb3000000u};
inline std::uint32_t next(std::uint32_t &seed) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}
template<class Accept> bool probe(Accept accept) {
    // Each boundary occupies each channel against all other boundaries, then
    // whole quadruples cover cancellation, negative weights and alias cases.
    for (auto first : boundary) for (auto second : boundary)
        for (unsigned channel=0; channel<4; ++channel) {
            Bits values{second,second,second,second};
            values[channel]=first;
            if (!accept(values)) return false;
        }
    std::uint32_t seed=0xb02a3c47u;
    for (unsigned index=0; index<4096; ++index) {
        Bits values{};
        for (auto &value : values) {
            const auto raw=next(seed);
            value=(raw & 0x80000000u) | ((raw & 0x7fffffffu) % 0x3f800001u);
        }
        if (!accept(values)) return false;
    }
    return true;
}
template<class Accept> bool extended(Accept accept) {
    for (auto a : boundary) for (auto b : boundary) for (auto c : boundary) for (auto d : boundary)
        if (!accept(Bits{a,b,c,d})) return false;
    // Dense non-tiny pairs complement the exponent-heavy probe distribution.
    std::uint32_t seed=0x5e29b0adu;
    for (unsigned index=0; index<65536; ++index) {
        Bits values{};
        for (auto &value : values) {
            const auto raw=next(seed);
            value=(raw & 0x80000000u) | (0x3e000000u + (raw % 0x01800001u));
        }
        if (!accept(values)) return false;
    }
    return true;
}
} // namespace direction_blend_cases
