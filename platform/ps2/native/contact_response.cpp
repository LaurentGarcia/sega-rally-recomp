#include "contact_response.h"
#include "contact_normalization.h"
#include "rounded_cross_product.h"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
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
    // The rounded length is normal, within the declared positive normal domain. For normal
    // quotients shift=23-ratio_top; underflow has a smaller numerator.
    // Scaling therefore needs at most 48 bits, with no negative shift.
    const auto numerator=std::uint64_t(a.numerator)<<shift;
    auto significand=numerator/b.numerator;
    const auto remainder=numerator%b.numerator;
    const auto twice=remainder*2;
    if(twice>b.numerator||(twice==b.numerator&&(significand&1)))++significand;
    return rounded(significand,quantum,sign);
}

// Restoring integer square root. The input has at most 48 bits, and the
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
std::uint32_t bits(float value) noexcept { return std::bit_cast<std::uint32_t>(value); }
float value(std::uint32_t word) noexcept { return std::bit_cast<float>(word); }
bool bounded(std::uint32_t word,std::uint32_t limit) noexcept { return (word&magnitude_mask)<=limit; }
bool greater_equal(std::uint32_t a,std::uint32_t b) noexcept {
    if(!(a&magnitude_mask)&&!(b&magnitude_mask))return true;
    if((a^b)&sign_bit)return !(a&sign_bit);
    return a&sign_bit?a<=b:a>=b;
}
} // namespace

bool make_contact_response(const ContactResponseInput& input,float previous_scalar,ContactResponseOutput& output) noexcept {
    ContactResponseOutput result{};
    if(!(input.contact.status&0x80u)) {
        std::memcpy(&result.selected_scalar,&previous_scalar,sizeof previous_scalar);
        std::memcpy(&output,&result,sizeof result);return true;
    }
    const std::array<std::uint32_t,3> direction{bits(input.contact.direction[0]),bits(input.contact.direction[1]),bits(input.contact.direction[2])};
    const std::array<std::uint32_t,3> offset{bits(input.offset.x),bits(input.offset.y),bits(input.offset.z)};
    for(unsigned i=0;i<3;++i)if(!bounded(direction[i],0x41800000u)||!bounded(offset[i],0x41800000u))return false;
    const auto gap=bits(input.contact.gap),previous=bits(previous_scalar),p=bits(input.parameter50),q=bits(input.parameter54);
    if(!bounded(gap,0x44800000u)||!bounded(previous,0x44800000u)||
       p<0x3a800000u||p>0x47800000u||q<0x3a800000u||q>0x47800000u)return false;
    const CrossVector3 normal{value(direction[0]),value(direction[1]),value(direction[2])};
    const CrossVector3 point{value(offset[0]),value(offset[1]),value(offset[2])};
    CrossVector3 first,second;
    if(!rounded_cross_product(normal,point,first)||!rounded_cross_product(first,normal,second))return false;
    ContactDirection3 unit;
    if(!normalize_contact_direction({second.x,second.y,second.z},unit))return false;
    const auto dot=sum(sum(product(bits(unit.x),offset[0]),product(bits(unit.y),offset[1])),product(bits(unit.z),offset[2]));
    if(dot&sign_bit && dot&magnitude_mask)return false;
    const auto length=root(dot&magnitude_mask);
    const auto denominator=sum(q,product(product(length,p),length));
    const auto numerator=product(p,q);
    // The numerator is normal in [2^-20,2^32]. Halving is exact in either
    // format. A quotient of two 24-bit significands rounded first to53 bits
    // and then24 cannot differ from direct nearest-even binary32 rounding
    // here: the result is normal and any nonzero midpoint distance is larger
    // than the binary64 rounding interval.
    const auto half=numerator-0x00800000u;
    const auto gain=product(quotient(half,denominator),gap);
    result.response={value(product(gain,direction[0])),value(product(gain,direction[1])),value(product(gain,direction[2]))};
    result.selected_scalar=value(greater_equal(gap,previous)?gap:previous);
    result.active=true;
    std::memcpy(&output,&result,sizeof result);
    return true;
}
} // namespace ps2::native
