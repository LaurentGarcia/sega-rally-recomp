#pragma once
#include "recovered_scalar_metric.h"
#include <array>
#include <bit>
#include <cstdint>

namespace recovered_scalar_metric_cases {
using Case=std::array<std::uint32_t,6>;
inline ps2::native::MetricPoint3 point(const Case &c,unsigned offset) {
    return {std::bit_cast<float>(c[offset]),std::bit_cast<float>(c[offset+1]),std::bit_cast<float>(c[offset+2])};
}
inline std::uint32_t next(std::uint32_t &state) {
    state^=state<<13;state^=state>>17;state^=state<<5;return state;
}
template<class Test> bool each(Test test,bool extended) {
    for(unsigned mask=0;mask<64;++mask) {
        Case c{};for(unsigned i=0;i<6;++i)c[i]=((mask>>i)&1u)<<31;
        if(!test(c))return false;
    }
    // Every permitted coordinate exponent, including all subnormal/zero
    // classes; fractions straddle the square's exponent transition near sqrt2.
    constexpr std::array<std::uint32_t,8> fractions{0,1,0x3504f2,0x3504f3,0x3504f4,0x400000,0x7ffffe,0x7fffff};
    for(unsigned exponent=0;exponent<=147;++exponent)
        for(auto fraction:fractions)for(unsigned lane=0;lane<3;++lane) {
            const auto word=(exponent<<23)|fraction;
            Case c{};c[3+lane]=word;if(!test(c))return false;
            c[lane]=word^0x80000000u;if(!test(c))return false;
        }
    constexpr std::array<std::uint32_t,18> boundaries{0,0x80000000u,1,0x80000001u,
        0x007fffffu,0x00800000u,0x19ffffffu,0x1a000000u,0x1f800000u,0x1fffffffu,
        0x20000000u,0x20000001u,0x3f7fffffu,0x3f800000u,0x3f800001u,
        0xbf800000u,0x497fffffu,0x49800000u};
    for(auto a:boundaries)for(auto b:boundaries) {
        if(!test(Case{a,b,a^0x80000000u,b,a,b^0x80000000u}))return false;
    }
    // Uniform coordinate-mantissa strata at six scales exercise both parity
    // paths and near-minimum/maximum sums. This is sampling, not exhaustive.
    const unsigned strata=extended?8192:64;
    for(unsigned exponent:{64u,65u,126u,127u,146u,147u})
        for(unsigned i=0;i<strata;++i) {
            Case c{};c[3]=(exponent<<23)|(i*(0x800000u/strata));
            if(!test(c))return false;
        }
    std::uint32_t state=0x65862e19u;
    for(unsigned i=0;i<(extended?100000u:4096u);++i) {
        Case c;for(auto &v:c){const auto r=next(state);v=(r&0x80000000u)|((r&0x7fffffffu)%0x49800001u);}
        if(i%7==0)for(unsigned j=0;j<3;++j)c[3+j]=c[j];
        if(i%11==0){c[1]=0;c[4]=0;}
        if(!test(c))return false;
    }
    return true;
}
inline void hash(std::uint64_t &value,const Case &input,bool accepted,std::uint32_t output) {
    for(auto word:input)value=(value^word)*1099511628211ull;
    value=(value^std::uint32_t(accepted))*1099511628211ull;
    value=(value^output)*1099511628211ull;
}
} // namespace recovered_scalar_metric_cases
