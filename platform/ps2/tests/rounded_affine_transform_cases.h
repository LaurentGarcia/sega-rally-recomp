#pragma once
#include "rounded_affine_transform.h"
#include <bit>
#include <cstdint>

namespace rounded_affine_cases {
using Case = std::array<std::uint32_t, 15>;
using Words = std::array<std::uint32_t, 3>;
inline ps2::native::AffinePoint3 point(const Case &c, unsigned offset) {
    return {std::bit_cast<float>(c[offset]),std::bit_cast<float>(c[offset+1]),
            std::bit_cast<float>(c[offset+2])};
}
inline ps2::native::AffineTransform3 transform(const Case &c) {
    return {{point(c,0),point(c,3),point(c,6)},point(c,9)};
}
inline Words bits(const ps2::native::AffinePoint3 &p) {
    return {std::bit_cast<std::uint32_t>(p.x),std::bit_cast<std::uint32_t>(p.y),
            std::bit_cast<std::uint32_t>(p.z)};
}
inline void hash(std::uint64_t &value, const Case &input, const Words &output) {
    for (auto word : input) value = (value ^ word) * 1099511628211ull;
    for (auto word : output) value = (value ^ word) * 1099511628211ull;
}
template<class F> bool each(F test, bool extended) {
    constexpr std::array<std::uint32_t,32> edges{
        0,0x80000000u,1,0x80000001u,2,0x80000002u,0x007fffffu,0x807fffffu,
        0x00800000u,0x80800000u,0x00800001u,0x80800001u,
        0x3f000000u,0xbf000000u,0x3f7fffffu,0xbf7fffffu,
        0x3f800000u,0xbf800000u,0x3f800001u,0xbf800001u,
        0x33800000u,0xb3800000u,0x337fffffu,0xb37fffffu,
        0x49800000u,0xc9800000u,0x497fffffu,0xc97fffffu,
        0x35800000u,0xb5800000u,0x20000000u,0xa0000000u};
    // Exhaust every sign combination of zero among all 15 inputs in the host
    // run; the same evenly spaced subset forms the bounded console fixture.
    for (unsigned mask=0;mask<32768;mask+=extended?1:64) {
        Case c{};
        for(unsigned i=0;i<c.size();++i)c[i]=(mask&(1u<<i))?0x80000000u:0;
        if(!test(c))return false;
    }
    for (unsigned a=0;a<edges.size();++a) for(unsigned b=0;b<edges.size();++b) {
        Case c{};
        for(unsigned i=0;i<12;++i)c[i]=edges[(a+7*i)%edges.size()];
        for(unsigned i=0;i<3;++i)c[12+i]=edges[(b+11*i)%edges.size()];
        if(!test(c))return false;
    }
    for (unsigned lane=0;lane<15;++lane) for(auto value:edges) {
        Case c{}; c[0]=c[4]=c[8]=0x3f800000u;c[12]=c[13]=c[14]=0x3f800000u;
        c[lane]=value;if(!test(c))return false;
    }
    std::uint32_t random=0x91b734a5u;
    auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for(unsigned i=0;i<(extended?50000u:2048u);++i) {
        Case c{};
        for(auto &value:c) {const auto raw=next(); value=(raw&0x80000000u)|((raw&0x7fffffffu)%0x49800001u);}
        if(!test(c))return false;
    }
    // Translation-first cancellation differs from computing a dot product and
    // adding translation last; the original ordered sum returns positive zero.
    Case order{};order[0]=0xbf800000u;order[3]=0x3f800000u;
    order[9]=0x33000000u;order[12]=order[13]=0x3f800000u;
    return test(order);
}
} // namespace rounded_affine_cases
