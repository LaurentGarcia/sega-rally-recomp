#include "vehicle_arithmetic.h"
#include <array>
#include <bit>
#include <limits>

namespace ps2::native::vehicle_math {
namespace {
constexpr std::uint32_t sign32=0x80000000u,invalid32=0x7fc00000u;
constexpr std::uint64_t leading=std::uint64_t(1)<<52;
using Wide=std::array<std::uint32_t,4>;
Number invalid() noexcept { return {0,0,false,false}; }
std::uint32_t bits(float value) noexcept { return std::bit_cast<std::uint32_t>(value); }
float value(std::uint32_t word) noexcept { return std::bit_cast<float>(word); }
bool bit(const Wide& words,unsigned n) noexcept {return (words[n/32]>>(n%32))&1;}
int top(const Wide& words) noexcept {
    for(int i=3;i>=0;--i)if(words[i])return 32*i+int(std::bit_width(words[i]))-1;
    return -1;
}
// Move whole limbs while retaining every discarded bit for the existing
// nearest/even rounding. The part guards avoid shifts by the word width.
Wide shifted(std::uint64_t word,unsigned shift) noexcept {
    Wide out{};
    if(shift>=128)return out;
    const unsigned index=shift/32,part=shift%32;
    const auto low=std::uint32_t(word),high=std::uint32_t(word>>32);
    out[index]=low<<part;
    if(index+1<4)out[index+1]=(high<<part)|(part?low>>(32-part):0u);
    if(part&&index+2<4)out[index+2]=high>>(32-part);
    return out;
}
std::uint64_t extract(const Wide& words,unsigned shift) noexcept {
    if(shift>=128)return 0;
    const unsigned index=shift/32,part=shift%32;
    std::uint64_t pair=words[index];
    if(index+1<4)pair|=std::uint64_t(words[index+1])<<32;
    auto out=pair>>part;
    if(part&&index+2<4)out|=std::uint64_t(words[index+2])<<(64-part);
    return out;
}
bool any_below(const Wide& words,unsigned count) noexcept {
    if(count>=128)return words[0]||words[1]||words[2]||words[3];
    const unsigned whole=count/32,part=count%32;
    for(unsigned i=0;i<whole;++i)if(words[i])return true;
    return part&&(words[whole]&((std::uint32_t(1)<<part)-1));
}
Number checked(Number n) noexcept {
    if(!n.valid)return invalid();
    if(!n.significand)return {0,0,n.negative,true};
    if(n.significand<leading||n.significand>=2*leading||n.power < -1074||n.power>971)
        return invalid();
    return n;
}
Number rounded(const Wide& words,int power,bool negative) noexcept {
    const int highest=top(words);
    if(highest<0)return {0,0,negative,true};
    const int shift=highest-52;
    std::uint64_t mantissa;
    if(shift>0){
        mantissa=extract(words,unsigned(shift));
        const bool guard=bit(words,unsigned(shift-1));
        const bool sticky=any_below(words,unsigned(shift-1));
        if(guard&&(sticky||(mantissa&1)))++mantissa;
        if(mantissa==2*leading){mantissa>>=1;++power;}
    }else mantissa=extract(words,0)<<(-shift);
    return checked({mantissa,power+shift,negative,true});
}
std::uint32_t single(std::uint64_t mantissa,int power,bool negative) noexcept {
    const auto sign=negative?sign32:0u;
    if(!mantissa)return sign;
    const int highest=int(std::bit_width(mantissa))-1;
    const int shift=highest+power < -126 ? -149-power : highest-23;
    if(shift>0){
        // All callers supply at most 53 significant bits. Beyond 63 discarded
        // bits the exact magnitude is strictly below half the minimum quantum.
        if(shift>=64)return sign;
        const auto discarded=mantissa&((std::uint64_t(1)<<shift)-1);
        const auto half=std::uint64_t(1)<<(shift-1);
        mantissa>>=shift;
        if(discarded>half||(discarded==half&&(mantissa&1)))++mantissa;
    }else mantissa<<=-shift;
    power+=shift;
    if(mantissa>=0x1000000u){mantissa>>=1;++power;}
    if(mantissa<0x800000u)return sign|std::uint32_t(mantissa);
    if(power+150>=255)return invalid32;
    return sign|(std::uint32_t(power+150)<<23)|(std::uint32_t(mantissa)&0x7fffffu);
}
}

Number from_single(float input) noexcept {
    const auto word=bits(input),exponent=(word>>23)&255u;
    if(exponent==255)return invalid();
    const auto mantissa=(word&0x7fffffu)|(exponent?0x800000u:0u);
    return rounded(shifted(mantissa,0),exponent?int(exponent)-150:-149,bool(word&sign32));
}
Number from_bits(std::uint64_t word) noexcept {
    const unsigned exponent=unsigned((word>>52)&2047u);
    if(!exponent)return (word&(leading-1))?invalid():Number{0,0,bool(word>>63),true};
    if(exponent==2047)return invalid();
    return {(word&(leading-1))|leading,int(exponent)-1075,bool(word>>63),true};
}
Number from_integer(std::int32_t integer) noexcept {
    const auto magnitude=integer<0?std::uint64_t(-std::int64_t(integer)):std::uint64_t(integer);
    return rounded(shifted(magnitude,0),0,integer<0);
}
Number negate(Number n) noexcept { n.negative=!n.negative;return n; }
Number add(Number a,Number b) noexcept {
    a=checked(a);b=checked(b);
    if(!a.valid||!b.valid)return invalid();
    if(!a.significand&&!b.significand)return {0,0,a.negative&&b.negative,true};
    if(!a.significand)return b;
    if(!b.significand)return a;
    if(a.power<b.power){const auto saved=a;a=b;b=saved;}
    const unsigned gap=unsigned(a.power-b.power);
    if(gap>54)return a;
    Wide first=shifted(a.significand,gap),second=shifted(b.significand,0),out{};
    bool negative=a.negative;
    if(a.negative==b.negative){
        std::uint64_t carry=0;
        for(unsigned i=0;i<4;++i){const auto sum=std::uint64_t(first[i])+second[i]+carry;out[i]=std::uint32_t(sum);carry=sum>>32;}
    }else{
        if(!gap&&a.significand==b.significand)return {};
        if(!gap&&a.significand<b.significand){first=shifted(b.significand,0);second=shifted(a.significand,0);negative=b.negative;}
        std::uint64_t borrow=0;
        for(unsigned i=0;i<4;++i){const auto sub=std::uint64_t(second[i])+borrow;out[i]=std::uint32_t(std::uint64_t(first[i])-sub);borrow=std::uint64_t(first[i])<sub;}
    }
    return rounded(out,b.power,negative);
}
Number subtract(Number a,Number b) noexcept {return add(a,negate(b));}
Number multiply(Number a,Number b) noexcept {
    a=checked(a);b=checked(b);
    if(!a.valid||!b.valid)return invalid();
    Wide product{};
    const std::array<std::uint32_t,2> x{std::uint32_t(a.significand),std::uint32_t(a.significand>>32)};
    const std::array<std::uint32_t,2> y{std::uint32_t(b.significand),std::uint32_t(b.significand>>32)};
    for(unsigned i=0;i<2;++i){
        std::uint64_t carry=0;
        for(unsigned j=0;j<2;++j){const auto sum=std::uint64_t(x[i])*y[j]+product[i+j]+carry;product[i+j]=std::uint32_t(sum);carry=sum>>32;}
        product[i+2]=std::uint32_t(carry);
    }
    return rounded(product,a.power+b.power,a.negative!=b.negative);
}
Number divide(Number a,Number b) noexcept {
    a=checked(a);b=checked(b);
    if(!a.valid||!b.valid||!b.significand)return invalid();
    const bool negative=a.negative!=b.negative;
    if(!a.significand)return {0,0,negative,true};
    const unsigned shift=a.significand<b.significand?53:52;
    const auto numerator=shifted(a.significand,shift);
    std::uint64_t quotient=0,remainder=0;
    for(int i=top(numerator);i>=0;--i){
        remainder=(remainder<<1)|unsigned(bit(numerator,unsigned(i)));
        quotient<<=1;
        if(remainder>=b.significand){remainder-=b.significand;quotient|=1;}
    }
    if(2*remainder>b.significand||(2*remainder==b.significand&&(quotient&1)))++quotient;
    int power=a.power-b.power-int(shift);
    if(quotient==2*leading){quotient>>=1;++power;}
    return checked({quotient,power,negative,true});
}
bool compare(Number a,Number b,int& order) noexcept {
    a=checked(a);b=checked(b);if(!a.valid||!b.valid)return false;
    if(!a.significand&&!b.significand){order=0;return true;}
    if(a.negative!=b.negative){order=a.negative?-1:1;return true;}
    int magnitude;
    if(!a.significand)magnitude=-1;
    else if(!b.significand)magnitude=1;
    else if(a.power!=b.power)magnitude=a.power>b.power?1:-1;
    else magnitude=(a.significand>b.significand)-(a.significand<b.significand);
    order=a.negative?-magnitude:magnitude;return true;
}
bool truncate(Number n,std::int32_t& output) noexcept {
    n=checked(n);if(!n.valid)return false;
    if(!n.significand||n.power < -52){output=0;return true;}
    if(n.power > -21)return false;
    const auto magnitude=n.significand>>(-n.power);
    const auto maximum=n.negative?0x80000000ull:0x7fffffffull;
    if(magnitude>maximum)return false;
    output=std::int32_t(n.negative?-std::int64_t(magnitude):std::int64_t(magnitude));return true;
}
float narrow(Number n) noexcept {
    n=checked(n);return value(n.valid?single(n.significand,n.power,n.negative):invalid32);
}
bool finite(float n) noexcept {return (bits(n)&0x7fffffffu)<0x7f800000u;}
float add32(float a,float b) noexcept {return narrow(add(from_single(a),from_single(b)));}
float sub32(float a,float b) noexcept {return narrow(subtract(from_single(a),from_single(b)));}
float mul32(float a,float b) noexcept {return narrow(multiply(from_single(a),from_single(b)));}
float div32(float a,float b) noexcept {return narrow(divide(from_single(a),from_single(b)));}
float sqrt32(float n) noexcept {
    const auto word=bits(n),magnitude=word&0x7fffffffu;
    if(!magnitude)return n;
    if((word&sign32)||magnitude>=0x7f800000u)return value(invalid32);
    const unsigned e=(word>>23)&255u;
    const auto mantissa=(word&0x7fffffu)|(e?0x800000u:0u);
    const int power=e?int(e)-150:-149;
    const int exponent=int(std::bit_width(mantissa))-1+power;
    const int half=exponent>=0?exponent/2:-((-exponent+1)/2);
    const int quantum=half-23;
    const auto radicand=std::uint64_t(mantissa)<<(power-2*quantum);
    std::uint64_t remainder=radicand,result=0,step=std::uint64_t(1)<<46;
    while(step){if(remainder>=result+step){remainder-=result+step;result=(result>>1)+step;}else result>>=1;step>>=2;}
    if(radicand-result*result>result)++result;
    return value(single(result,quantum,false));
}
} // namespace ps2::native::vehicle_math
