#include "recovered_atangent.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {
unsigned checks=0;std::uint64_t hash=14695981039346656037ull;
bool check(std::uint32_t bits){
    const auto input=std::bit_cast<float>(bits);float result=17;
    const bool success=ps2::native::recovered_atangent(input,result);
    if((bits&0x7fffffffu)>=0x7f800000u)return !success&&result==17;
    // Same documented host-double atan2 boundary as the offline reference.
    const volatile double expected_double=std::atan2(double(input),1.0);
    const float expected=float(expected_double);
    const auto actual=std::bit_cast<std::uint32_t>(result),wanted=std::bit_cast<std::uint32_t>(expected);
    if(!success||actual!=wanted){std::fprintf(stderr,"atan input=%08x wanted=%08x actual=%08x accepted=%u\n",bits,wanted,actual,success);return false;}
    ++checks;for(unsigned i=0;i<4;++i){hash^=(actual>>(8*i))&255;hash*=1099511628211ull;}return true;
}
}
int main(){
    for(auto center:{0u,0x00800000u,0x39000000u,0x3f000000u,0x3f800000u,0x40000000u,0x7f7fffffu})
        for(int delta=-128;delta<=128;++delta){const auto bits=center+std::uint32_t(delta);
            if(!check(bits)||!check(bits^0x80000000u))return 1;}
    std::uint32_t seed=0x78ab2145;
    for(unsigned i=0;i<300000;++i){seed=seed*1664525u+1013904223u;if(!check(seed))return 1;}
    // Dense representable grid exercises the driving input range and both signs.
    for(int i=-131072;i<=131072;++i)if(!check(std::bit_cast<std::uint32_t>(float(i)/65536.0f)))return 1;
    for(auto bits:{0x7f800000u,0xff800000u,0x7fc00000u,0xffc00001u})if(!check(bits))return 1;
    std::printf("RECOVERED_ATANGENT PASS comparisons=%u hash=%016llx\n",checks,(unsigned long long)hash);
}
