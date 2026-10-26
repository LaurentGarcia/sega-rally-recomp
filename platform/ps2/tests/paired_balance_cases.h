#pragma once
#include "paired_balance.h"
#include <bit>
#include <cstdint>

namespace paired_balance_cases {
using Input=std::array<float,4>;
using Words=std::array<std::uint32_t,4>;
inline float value(std::uint32_t word){return std::bit_cast<float>(word);}
inline std::uint32_t bits(float v){return std::bit_cast<std::uint32_t>(v);}
inline Words words(const ps2::native::PairedBalance& out){return {bits(out.sums[0]),bits(out.sums[1]),bits(out.balances[0]),bits(out.balances[1])};}
inline void hash(std::uint64_t& h,const Words& words){for(auto v:words)h=(h^v)*1099511628211ull;}
inline std::uint32_t next(std::uint32_t& state){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
inline std::uint32_t finite(std::uint32_t& state){const auto word=next(state);return (word&0x80000000u)|((word&0x7fffffffu)%0x49800001u);}
inline ps2::native::PairedBalance sentinel(){return {{value(0x7fc11111),value(0xffc22222)},{value(0x7fc33333),value(0xffc44444)}};}
template<class F> bool each(F&& f,bool extended){
    constexpr std::array<std::uint32_t,20> boundary{0,0x80000000u,1,0x80000001u,0x7fffffu,0x807fffffu,
        0x800000u,0x80800000u,0x800001u,0x80800001u,0x3f000000u,0xbf000000u,0x3f7fffffu,0x3f800000u,
        0x3f800001u,0xbf7fffffu,0xbf800000u,0xbf800001u,0x49800000u,0xc9800000u};
    for(auto a:boundary)for(auto b:boundary)if(!f(Input{value(a),value(b),value(b),value(a)}))return false;
    for(unsigned e=1;e<=146;++e){const std::uint32_t a=std::uint32_t(e)<<23;
        for(int offset=-2;offset<=2;++offset){const auto b=std::uint32_t(std::int64_t(a)+offset);
            if(!f(Input{value(a),value(b^0x80000000u),value(a^0x80000000u),value(b)}))return false;
        }
    }
    std::uint32_t state=0x97d56e23u;
    for(unsigned i=0;i<(extended?100000u:1024u);++i){
        Input input{value(finite(state)),value(finite(state)),value(finite(state)),value(finite(state))};
        if((i%4)==0){const auto a=bits(input[0]);const auto mag=a&0x7fffffffu;
            input[1]=value(((a^0x80000000u)&0x80000000u)|((mag+1)%0x49800001u));
        }
        if(!f(input))return false;
    }
    return true;
}
#ifndef _EE
inline ps2::native::PairedBalance mathematical(const Input& input){
    ps2::native::PairedBalance result;
    for(unsigned i=0;i<2;++i){
        volatile float total=input[2*i]+input[2*i+1];result.sums[i]=total;
        if(total==0){result.balances[i]=0;continue;}
        volatile float difference=input[2*i+1]-input[2*i];
        volatile double product=double(difference)*std::bit_cast<double>(std::uint64_t(0x3ff199999999999aull));
        volatile double quotient=product/double(total);
        result.balances[i]=float(quotient);
    }
    return result;
}
#endif
} // namespace paired_balance_cases
