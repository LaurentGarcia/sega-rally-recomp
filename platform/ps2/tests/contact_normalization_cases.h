#pragma once
#include "contact_normalization.h"
#include <array>
#include <bit>
#include <cstdint>
namespace contact_normalization_cases {
using Words=std::array<std::uint32_t,3>;
inline ps2::native::ContactDirection3 vector(const Words &w) {
    return {std::bit_cast<float>(w[0]),std::bit_cast<float>(w[1]),std::bit_cast<float>(w[2])};
}
inline Words bits(const ps2::native::ContactDirection3 &v) {
    return {std::bit_cast<std::uint32_t>(v.x),std::bit_cast<std::uint32_t>(v.y),std::bit_cast<std::uint32_t>(v.z)};
}
inline void hash(std::uint64_t &state,const Words &input,const Words &output) {
    for(auto v:input)state=(state^v)*1099511628211ull;
    for(auto v:output)state=(state^v)*1099511628211ull;
}
template<class Visit>bool each(Visit visit,bool extended) {
    constexpr std::array<std::uint32_t,20> edges{0,0x80000000u,1,0x80000001u,0x7fffffu,0x807fffffu,
        0x800000u,0x80800000u,0x19ffffffu,0x99ffffffu,0x1a000000u,0x9a000000u,
        0x1a000001u,0x9a000001u,0x3f800000u,0xbf800000u,0x49800000u,0xc9800000u,
        0x3f7fffffu,0xbf800001u};
    const unsigned stride=extended?1:2;
    for(unsigned x=0;x<edges.size();x+=stride)
        for(unsigned y=0;y<edges.size();y+=stride)
            for(unsigned z=0;z<edges.size();z+=stride)
                if(!visit(Words{edges[x],edges[y],edges[z]}))return false;
    for(unsigned exponent=0;exponent<148;++exponent)
        for(unsigned mode=0;mode<6;++mode) {
            const auto middle=std::uint32_t(exponent)<<23;
            Words c{middle,middle|0x80000000u,middle};
            c[mode%3]|=(mode<3?1u:0x7fffffu);
            // exponent147 includes values beyond2^20; retain its exact endpoint.
            for(auto &v:c)if((v&0x7fffffffu)>0x49800000u)v=(v&0x80000000u)|0x49800000u;
            if(!visit(c))return false;
        }
    for(unsigned i=0;i<(extended?1024u:128u);++i)
        if(!visit(Words{0x3f800000u+i,0x3f800000u-i,0x33800000u+(i<<8)}))return false;
    std::uint32_t random=0x9184e3b1u;
    auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for(unsigned i=0;i<(extended?50000u:2048u);++i) {
        Words c;
        for(auto &v:c) {const auto n=next();v=(n&0x80000000u)|((n&0x7fffffffu)%0x49800001u);}
        if(!visit(c))return false;
    }
    return true;
}
} // namespace contact_normalization_cases
