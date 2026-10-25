#include "contact_normalization.h"
#include <array>
#include <bit>
#include <cstdint>
#include <limits>

namespace ps2::native {
namespace {
constexpr std::uint32_t sign_bit=0x80000000u, magnitude_mask=0x7fffffffu;
struct Dyadic { std::uint32_t numerator; int power; };
Dyadic parts(std::uint32_t bits) noexcept {
    const auto exponent=(bits>>23)&255u;
    return {(bits&0x7fffffu)|(exponent?0x800000u:0u),exponent?int(exponent)-150:-149};
}
// Original bounded integer arithmetic shared conceptually with our projected
// contact implementation. Only finite, range-checked operations enter here.
std::uint32_t rounded(std::uint64_t numerator,int power,std::uint32_t sign) noexcept {
    if(!numerator)return sign;
    const int top=int(std::bit_width(numerator))-1;
    const int shift=top+power < -126 ? -149-power : top-23;
    if(shift>0){
        if(shift>=64)return sign;
        const auto discarded=numerator&((std::uint64_t(1)<<shift)-1);
        const auto half=std::uint64_t(1)<<(shift-1);
        numerator>>=shift;
        if(discarded>half||(discarded==half&&(numerator&1)))++numerator;
    }else numerator<<=-shift;
    power+=shift;
    if(numerator>=0x1000000u){numerator>>=1;++power;}
    if(numerator<0x800000u)return sign|std::uint32_t(numerator);
    return sign|(std::uint32_t(power+150)<<23)|(std::uint32_t(numerator)&0x7fffffu);
}
std::uint32_t product(std::uint32_t left,std::uint32_t right) noexcept {
    const auto a=parts(left),b=parts(right);
    return rounded(std::uint64_t(a.numerator)*b.numerator,a.power+b.power,(left^right)&sign_bit);
}
std::uint32_t sum(std::uint32_t left,std::uint32_t right) noexcept {
    auto a=parts(left),b=parts(right);
    if(!a.numerator&&!b.numerator)return (left&right)&sign_bit;
    if(!a.numerator)return right;
    if(!b.numerator)return left;
    if(a.power<b.power){const auto bits=left;left=right;right=bits;const auto old=a;a=b;b=old;}
    const unsigned gap=unsigned(a.power-b.power);
    if(gap>32)return left;
    const auto first=std::uint64_t(a.numerator)<<gap,second=std::uint64_t(b.numerator);
    if((left^right)&sign_bit){
        if(first==second)return 0;
        return first>second?rounded(first-second,b.power,left&sign_bit):rounded(second-first,b.power,right&sign_bit);
    }
    return rounded(first+second,b.power,left&sign_bit);
}
std::uint32_t quotient(std::uint32_t left,std::uint32_t right) noexcept {
    const auto a=parts(left),b=parts(right);
    const auto sign=(left^right)&sign_bit;
    if(!a.numerator)return sign;
    int ratio_top=int(std::bit_width(a.numerator))-int(std::bit_width(b.numerator));
    if(ratio_top>=0){if(std::uint64_t(a.numerator)<(std::uint64_t(b.numerator)<<ratio_top))--ratio_top;}
    else if((std::uint64_t(a.numerator)<<-ratio_top)<b.numerator)--ratio_top;
    const int exponent=ratio_top+a.power-b.power;
    const int quantum=exponent < -126 ? -149 : exponent-23;
    const unsigned shift=unsigned(a.power-b.power-quantum);
    // The rounded length is normal, between 2^-75 and 2^21. For normal
    // quotients shift=23-ratio_top; underflow has a smaller numerator.
    // Scaling therefore needs at most48 bits, with no negative shift.
    const auto numerator=std::uint64_t(a.numerator)<<shift;
    auto significand=numerator/b.numerator;
    const auto remainder=numerator%b.numerator;
    const auto twice=remainder*2;
    if(twice>b.numerator||(twice==b.numerator&&(significand&1)))++significand;
    return rounded(significand,quantum,sign);
}

// Restoring integer square root. The input has at most48 bits, and the
// remainder tests the exact half-way boundary without a floating estimate.
std::uint64_t integer_root(std::uint64_t value) noexcept {
    std::uint64_t result=0,step=std::uint64_t(1)<<46;
    while(step) {
        if(value>=result+step) {value-=result+step;result=(result>>1)+step;}
        else result>>=1;
        step>>=2;
    }
    return result;
}
std::uint32_t root(std::uint32_t bits) noexcept {
    if(!bits)return 0;
    const auto value=parts(bits);
    const int exponent=int(std::bit_width(value.numerator))-1+value.power;
    const int half=exponent>=0?exponent/2:-((-exponent+1)/2);
    const int quantum=half-23;
    const auto radicand=std::uint64_t(value.numerator)<<(value.power-2*quantum);
    auto significand=integer_root(radicand);
    // (n+1/2)^2=n*n+n+1/4. An integer radicand cannot tie, so the
    // remainder being greater than n is exactly the nearest-rounding test.
    if(radicand-significand*significand>significand)++significand;
    return rounded(significand,quantum,0);
}
} // namespace

bool normalize_contact_direction(const ContactDirection3 &input,
                                 ContactDirection3 &output) noexcept {
    static_assert(sizeof(float)==4 && std::numeric_limits<float>::radix==2 &&
                  std::numeric_limits<float>::digits==24);
    const std::array<std::uint32_t,3> words{std::bit_cast<std::uint32_t>(input.x),
        std::bit_cast<std::uint32_t>(input.y),std::bit_cast<std::uint32_t>(input.z)};
    for(auto word:words)if((word&magnitude_mask)>0x49800000u)return false;
    const auto squared=sum(sum(product(words[0],words[0]),product(words[1],words[1])),
                           product(words[2],words[2]));
    // The original binary64 threshold is 0x380fffffff9fdba8 (~2^-126).
    // Every nonzero binary32 squared sum has root >=sqrt(2^-149)>2^-75;
    // only a zero sum can reach the fallback, throughout this bounded domain.
    if(!squared) {output={0,0,1};return true;}
    const auto length=root(squared);
    output={std::bit_cast<float>(quotient(words[0],length)),
            std::bit_cast<float>(quotient(words[1],length)),
            std::bit_cast<float>(quotient(words[2],length))};
    return true;
}
} // namespace ps2::native
