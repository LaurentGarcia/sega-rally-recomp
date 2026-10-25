#pragma once
#include <array>
#include <bit>
#include <cstdint>
namespace contact_projection_cases {
struct Case { unsigned count; std::array<std::uint32_t, 10> values; };
inline std::uint32_t bits(float value) { return std::bit_cast<std::uint32_t>(value); }
template<class Test> bool each(Test test, bool extended) {
    // Original unit shapes, both windings, their edges/corners and outside points.
    for (unsigned count : {3u, 4u}) for (unsigned reverse : {0u, 1u})
        for (int x = -3; x <= 3; ++x) for (int z = -3; z <= 3; ++z) {
            Case c{count, {bits(-1), bits(-1), bits(1), bits(-1),
                           bits(1), bits(1), bits(-1), bits(1), bits(float(x)/2), bits(float(z)/2)}};
            if (reverse) for (unsigned i = 0; i < count/2; ++i) {
                for (unsigned k = 0; k < 2; ++k) {
                    auto &a = c.values[2*i+k], &b = c.values[2*(count-i-1)+k];
                    const auto saved = a; a = b; b = saved;
                }
            }
            if (!test(c)) return false;
        }
    constexpr std::uint32_t boundary[]{0,0x80000000u,1,0x80000001u,0x7fffffu,0x807fffffu,
        0x800000u,0x80800000u,0x3f7fffffu,0x3f800000u,0x3f800001u,
        0xbf7fffffu,0xbf800000u,0xbf800001u,0x497fffffu,0x49800000u,0xc9800000u};
    for (unsigned count : {3u,4u}) for (auto a : boundary) for (auto b : boundary) {
        Case c{count,{a, b, b, a, a ^ 0x80000000u, b, b, a ^ 0x80000000u, 0, 0}};
        if (!test(c)) return false;
    }
    std::uint32_t state=0x6d28ea91u;
    const auto random=[&](){state^=state<<13;state^=state>>17;state^=state<<5;return state;};
    const unsigned limit = extended ? 100000 : 4096;
    for (unsigned i=0;i<limit;++i) {
        Case c{3+(i&1u),{}};
        for (auto &v:c.values) { const auto x=random(); v=(x&0x80000000u)|((x&0x7fffffffu)%0x49800001u); }
        // Frequently approach a vertex or collapse an edge, including signed zeros.
        if(i%5==0){c.values[8]=c.values[0];c.values[9]=c.values[1];}
        if(i%7==0){c.values[2]=c.values[0];c.values[3]=c.values[1];}
        if(!test(c))return false;
    }
    return true;
}
} // namespace contact_projection_cases
