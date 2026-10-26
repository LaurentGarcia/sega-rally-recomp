#include "paired_balance.h"
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>

namespace ps2::native {
namespace {
constexpr std::uint32_t sign_bit=0x80000000u, magnitude=0x7fffffffu;
struct Dyadic { std::uint32_t significand; int power; };
Dyadic parts(std::uint32_t value) noexcept {
    const auto exponent=(value>>23)&255u;
    return {(value&0x7fffffu)|(exponent?0x800000u:0),exponent?int(exponent)-150:-149};
}
std::uint32_t single(std::uint64_t value,int power,std::uint32_t sign) noexcept {
    if(!value)return sign;
    const int top=int(std::bit_width(value))-1;
    const int shift=top+power < -126 ? -149-power : top-23;
    if(shift>0) {
        if(shift>=64)return sign;
        const auto tail=value&((std::uint64_t(1)<<shift)-1),half=std::uint64_t(1)<<(shift-1);
        value>>=shift;
        if(tail>half||(tail==half&&(value&1)))++value;
    } else value<<=-shift;
    power+=shift;
    if(value>=0x1000000u){value>>=1;++power;}
    if(value<0x800000u)return sign|std::uint32_t(value);
    return sign|(std::uint32_t(power+150)<<23)|(std::uint32_t(value)&0x7fffffu);
}
std::uint32_t sum(std::uint32_t left,std::uint32_t right) noexcept {
    auto a=parts(left),b=parts(right);
    if(!a.significand&&!b.significand)return (left&right)&sign_bit;
    if(!a.significand)return right;
    if(!b.significand)return left;
    if(a.power<b.power){const auto old=left;left=right;right=old;const auto p=a;a=b;b=p;}
    const unsigned shift=unsigned(a.power-b.power);
    if(shift>32)return left;
    const auto first=std::uint64_t(a.significand)<<shift,second=std::uint64_t(b.significand);
    if((left^right)&sign_bit){
        if(first==second)return 0;
        return first>second?single(first-second,b.power,left&sign_bit):single(second-first,b.power,right&sign_bit);
    }
    return single(first+second,b.power,left&sign_bit);
}

// Three little-endian limbs hold the largest exact 53x24-bit product and
// the scaled numerator of its following 53-bit quotient. This is bounded
// arithmetic for this expression, not a floating processor implementation.
using Wide=std::array<std::uint32_t,3>;
Wide multiply(std::uint64_t left,std::uint32_t right) noexcept {
    const auto low=std::uint64_t(std::uint32_t(left))*right;
    const auto high=(left>>32)*right+(low>>32);
    return {std::uint32_t(low),std::uint32_t(high),std::uint32_t(high>>32)};
}
Wide shifted(std::uint64_t value,unsigned shift) noexcept {
    if(!shift)return {std::uint32_t(value),std::uint32_t(value>>32),0};
    return {std::uint32_t(value<<shift),std::uint32_t(value>>(32-shift)),std::uint32_t(value>>(64-shift))};
}
struct DoubleValue { std::uint64_t significand; int power; };
DoubleValue weighted(std::uint32_t value) noexcept {
    const auto p=parts(value);
    const auto exact=multiply(0x1199999999999aull,p.significand); // binary64(1.1)*2^52
    const unsigned top=exact[2]?64+std::bit_width(exact[2])-1:32+std::bit_width(exact[1])-1;
    const unsigned shift=top-52; // 0..24 for a nonzero binary32 significand.
    if(!shift)return {(std::uint64_t(exact[1])<<32)|exact[0],p.power-52};
    auto retained=(std::uint64_t(exact[2])<<(64-shift))|(std::uint64_t(exact[1])<<(32-shift))|(exact[0]>>shift);
    const auto tail=exact[0]&((std::uint32_t(1)<<shift)-1),half=std::uint32_t(1)<<(shift-1);
    if(tail>half||(tail==half&&(retained&1)))++retained;
    int power=p.power-52+int(shift);
    if(retained==(std::uint64_t(1)<<53)){retained>>=1;++power;}
    return {retained,power};
}
std::uint32_t balance(std::uint32_t difference,std::uint32_t total) noexcept {
    const auto sign=(difference^total)&sign_bit;
    if(!(difference&magnitude))return sign;
    const auto a=weighted(difference);
    const auto b=parts(total);
    int ratio_top=int(std::bit_width(a.significand))-int(std::bit_width(b.significand));
    if(a.significand<(std::uint64_t(b.significand)<<ratio_top))--ratio_top;
    const unsigned shift=unsigned(52-ratio_top); // 0..24, including subnormal divisor.
    const auto numerator=shifted(a.significand,shift);
    Wide quotient{};std::uint64_t remainder=0;
    for(int i=2;i>=0;--i){
        const auto partial=(remainder<<32)|numerator[unsigned(i)];
        quotient[unsigned(i)]=std::uint32_t(partial/b.significand);
        remainder=partial%b.significand;
    }
    auto rounded=(std::uint64_t(quotient[1])<<32)|quotient[0];
    if(remainder*2>b.significand||(remainder*2==b.significand&&(rounded&1)))++rounded;
    return single(rounded,a.power-b.power-int(shift),sign);
}
} // namespace

bool make_paired_balance(const std::array<float,4>& values,PairedBalance& output) noexcept {
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::radix==2&&std::numeric_limits<float>::digits==24);
    std::array<std::uint32_t,4> words{};
    for(unsigned i=0;i<4;++i){words[i]=std::bit_cast<std::uint32_t>(values[i]);if((words[i]&magnitude)>0x49800000u)return false;}
    PairedBalance result;
    for(unsigned i=0;i<2;++i){
        const auto total=sum(words[2*i],words[2*i+1]);
        const auto difference=sum(words[2*i+1],words[2*i]^sign_bit);
        const auto reduced=(total&magnitude)?balance(difference,total):0u;
        result.sums[i]=std::bit_cast<float>(total);result.balances[i]=std::bit_cast<float>(reduced);
    }
    std::memcpy(&output,&result,sizeof result);return true;
}
} // namespace ps2::native
