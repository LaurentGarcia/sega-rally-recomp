#include "vehicle_arithmetic.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <limits>

namespace vm=ps2::native::vehicle_math;
namespace {
std::uint64_t random_state=0xc19532fa840771d3ull;
std::uint64_t next(){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return random_state;}
std::uint32_t bits(float v){return std::bit_cast<std::uint32_t>(v);}
std::uint64_t bits(double v){return std::bit_cast<std::uint64_t>(v);}
float single(std::uint32_t word){return std::bit_cast<float>(word);}
double wide(std::uint64_t word){return std::bit_cast<double>(word);}
std::uint64_t encoding(vm::Number n){
    if(!n.valid)return 0x7ff8000000000000ull;
    const auto sign=std::uint64_t(n.negative)<<63;
    return n.significand?sign|(std::uint64_t(n.power+1075)<<52)|(n.significand&0xfffffffffffffull):sign;
}
unsigned checks=0;
bool same32(const char*name,float actual,float expected){
    ++checks;
    if(std::isinf(expected)||std::isnan(expected))return !vm::finite(actual);
    if(bits(actual)==bits(expected))return true;
    std::fprintf(stderr,"VEHICLE_MATH %s actual=%08x expected=%08x check=%u\n",name,bits(actual),bits(expected),checks);return false;
}
bool same64(const char*name,vm::Number actual,double expected){
    ++checks;
    if(encoding(actual)==bits(expected))return true;
    std::fprintf(stderr,"VEHICLE_MATH %s actual=%016llx expected=%016llx check=%u\n",name,
        (unsigned long long)encoding(actual),(unsigned long long)bits(expected),checks);return false;
}
bool pair32(float a,float b){
    // Volatile stores and -ffp-contract=off supply an independent IEEE host
    // reference at each rounding boundary. This is not an original-game run.
    volatile float x=a,y=b;
    const float sum=x+y,difference=x-y,product=x*y;
    if(!same32("add32",vm::add32(a,b),sum)||!same32("sub32",vm::sub32(a,b),difference)||
       !same32("mul32",vm::mul32(a,b),product))return false;
    if((bits(b)&0x7fffffffu)&&!same32("div32",vm::div32(a,b),x/y))return false;
    const auto magnitude=single(bits(a)&0x7fffffffu);
    return same32("sqrt32",vm::sqrt32(magnitude),std::sqrt(magnitude));
}
}
int main(){
    if(std::fegetround()!=FE_TONEAREST||!std::numeric_limits<float>::is_iec559||
       !std::numeric_limits<double>::is_iec559)return 2;
    constexpr std::array<std::uint32_t,24> boundary={0,0x80000000u,1,2,3,0x007fffffu,
        0x00800000u,0x00800001u,0x33000000u,0x33800000u,0x3effffffu,0x3f000000u,
        0x3f000001u,0x3f7fffffu,0x3f800000u,0x3f800001u,0x40000000u,
        0x4affffffu,0x4b000000u,0x4b000001u,0x7f7fffffu,0xbf800000u,0x80800000u,0x80000001u};
    for(auto a:boundary)for(auto b:boundary)if(!pair32(single(a),single(b)))return 1;
    for(unsigned i=0;i<80000;++i){
        const auto a=std::uint32_t(next()),b=std::uint32_t(next());
        if((a&0x7fffffffu)<0x7f800000u&&(b&0x7fffffffu)<0x7f800000u)
            if(!pair32(single(a),single(b)))return 1;
    }
    for(unsigned i=0;i<20000;++i){
        const auto aw=(next()&0x800fffffffffffffull)|(std::uint64_t(723+next()%601)<<52);
        const auto bw=(next()&0x800fffffffffffffull)|(std::uint64_t(723+next()%601)<<52);
        volatile double a=wide(aw),b=wide(bw);
        const auto x=vm::from_bits(aw),y=vm::from_bits(bw);
        if(!same64("add64",vm::add(x,y),a+b)||!same64("sub64",vm::subtract(x,y),a-b)||
           !same64("mul64",vm::multiply(x,y),a*b)||!same64("div64",vm::divide(x,y),a/b)||
           !same32("narrow64",vm::narrow(x),float(a)))return 1;
        int order=77;
        if(!vm::compare(x,y,order)||order!=((a>b)-(a<b)))return 1;
    }
    for(auto integer:{std::numeric_limits<std::int32_t>::min(),-16777217,-1,0,1,16777217,std::numeric_limits<std::int32_t>::max()}){
        std::int32_t result=123;
        if(!same64("integer64",vm::from_integer(integer),double(integer))||
           !same32("integer32",vm::narrow(vm::from_integer(integer)),float(integer))||
           !vm::truncate(vm::from_integer(integer),result)||result!=integer)return 1;
    }
    for(float input:{-12.75f,-.5f,-0.f,0.f,.5f,12.75f}){
        std::int32_t result=123;if(!vm::truncate(vm::from_single(input),result)||result!=std::int32_t(input))return 1;
    }
    std::int32_t held=42;int order=42;
    if(vm::from_bits(1).valid||vm::from_bits(0x7ff0000000000000ull).valid||
       vm::from_single(single(0x7fc00001u)).valid||vm::finite(vm::div32(1,0))||
       vm::finite(vm::sqrt32(-1))||vm::truncate(vm::from_bits(0x41e0000000000000ull),held)||held!=42||
       vm::compare({0,0,false,false},vm::from_integer(1),order)||order!=42)return 1;
    if(bits(vm::sqrt32(-0.f))!=0x80000000u)return 1;
    std::printf("VEHICLE_ARITHMETIC PASS comparisons=%u\n",checks);
}
