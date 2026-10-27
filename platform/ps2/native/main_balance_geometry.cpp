#include "main_balance_geometry.h"
#include <bit>
#include <cstring>
#include <limits>
namespace ps2::native {
namespace {
// Independently written bounded integer rounding; no platform FP arithmetic.
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
struct Wide { std::array<std::uint32_t,4> words{}; };
struct Precise { std::uint64_t significand=0;int power=0;bool negative=false; };
int top(const Wide&v) noexcept {for(int i=3;i>=0;--i)if(v.words[i])return 32*i+int(std::bit_width(v.words[i]))-1;return -1;}
bool bit(const Wide&v,unsigned n) noexcept {return (v.words[n/32]>>(n%32))&1;}
Wide shifted(std::uint64_t v,unsigned n) noexcept {
    Wide result;for(unsigned i=0;i<64&&i+n<128;++i)if((v>>i)&1)result.words[(i+n)/32]|=std::uint32_t(1)<<((i+n)%32);return result;
}
std::uint64_t extract(const Wide&v,unsigned n) noexcept {std::uint64_t result=0;for(unsigned i=0;i<64&&i+n<128;++i)if(bit(v,i+n))result|=std::uint64_t(1)<<i;return result;}
Precise precise(Wide v,int power,bool negative) noexcept {
    const int highest=top(v);if(highest<0)return {0,0,negative};
    const int shift=highest-52;std::uint64_t mantissa;
    if(shift>0){
        mantissa=extract(v,unsigned(shift));const bool guard=bit(v,unsigned(shift-1));bool sticky=false;
        for(int i=0;i+1<shift;++i)sticky|=bit(v,unsigned(i));
        if(guard&&(sticky||(mantissa&1)))++mantissa;
        if(mantissa==0x20000000000000ull){mantissa>>=1;++power;}
    }else mantissa=extract(v,0)<<(-shift);
    return {mantissa,power+shift,negative};
}
Precise single(std::uint32_t word) noexcept {
    const auto v=parts(word);return precise(shifted(v.numerator,0),v.power,bool(word&sign_bit));
}
Precise constant(std::uint64_t word) noexcept {
    return {(word&0xfffffffffffffull)|0x10000000000000ull,int((word>>52)&2047)-1075,bool(word>>63)};
}
Precise multiply(Precise a,Precise b) noexcept {
    Wide product;const std::array<std::uint32_t,2> x{std::uint32_t(a.significand),std::uint32_t(a.significand>>32)},y{std::uint32_t(b.significand),std::uint32_t(b.significand>>32)};
    for(unsigned i=0;i<2;++i){std::uint64_t carry=0;for(unsigned j=0;j<2;++j){const auto sum=std::uint64_t(x[i])*y[j]+product.words[i+j]+carry;product.words[i+j]=std::uint32_t(sum);carry=sum>>32;}product.words[i+2]=std::uint32_t(carry);}
    return precise(product,a.power+b.power,a.negative!=b.negative);
}
Precise add(Precise a,Precise b) noexcept {
    if(!a.significand&&!b.significand)return {0,0,a.negative&&b.negative};
    if(!a.significand)return b;if(!b.significand)return a;
    if(a.power<b.power){const auto tmp=a;a=b;b=tmp;}
    const unsigned gap=unsigned(a.power-b.power);
    if(gap>54)return a;
    Wide first=shifted(a.significand,gap),second=shifted(b.significand,0),result;
    bool negative=a.negative;
    if(a.negative==b.negative){std::uint64_t carry=0;for(unsigned i=0;i<4;++i){const auto sum=std::uint64_t(first.words[i])+second.words[i]+carry;result.words[i]=std::uint32_t(sum);carry=sum>>32;}}
    else {
        if(!gap&&a.significand==b.significand)return {};
        if(!gap&&a.significand<b.significand){first=shifted(b.significand,0);second=shifted(a.significand,0);negative=b.negative;}
        std::uint64_t borrow=0;for(unsigned i=0;i<4;++i){const auto sub=std::uint64_t(second.words[i])+borrow;result.words[i]=std::uint32_t(std::uint64_t(first.words[i])-sub);borrow=std::uint64_t(first.words[i])<sub;}
    }
    return precise(result,b.power,negative);
}
Precise negate(Precise v) noexcept {v.negative=!v.negative;return v;}
std::uint32_t narrow(Precise v) noexcept {return rounded(v.significand,v.power,v.negative?sign_bit:0);}
// Recovered periodic polynomial. These are semantic numeric constants of the
// formula, not a lookup table or a copy of processor instructions. The two
// reductions and all six terms preserve their separate binary32 boundaries.
std::uint32_t periodic_polynomial(std::uint32_t angle) noexcept {
    constexpr std::uint32_t pi=0x40490fdbu,half_pi=0x3fc90fdbu,reciprocal_pi=0x3ea2f983u;
    constexpr std::array<std::uint32_t,6> coefficient{
        0x3f000000u,0x3d2aaaabu,0x3ab60b61u,0x37d00d01u,0x3493f27eu,0x310f76c7u};
    const auto magnitude=angle&magnitude_mask;
    const auto quotient=parts(product(magnitude,reciprocal_pi));
    const unsigned whole=quotient.power<=-32?0u:
        quotient.power<0?quotient.numerator>>(-quotient.power):quotient.numerator<<quotient.power;
    const auto multiple=product(pi,rounded(whole,0,0));
    const auto centered=sum(sum(magnitude,multiple^sign_bit),half_pi^sign_bit);
    const auto square=product(centered,centered);
    std::array<std::uint32_t,6> powers{square};
    for(unsigned i=1;i<6;++i)powers[i]=product(square,powers[i-1]);
    auto result=product(powers[5],coefficient[5]);
    for(int i=4;i>=0;--i){
        auto term=product(powers[unsigned(i)],coefficient[unsigned(i)]);
        if(!(i&1))term^=sign_bit;
        result=sum(result,term);
    }
    result=sum(result,0x3f800000u);
    const unsigned nonpositive=(angle&sign_bit)||!magnitude;
    return result^(((whole+nonpositive)&1u)?sign_bit:0u);
}

std::uint32_t bits(float v) noexcept {return std::bit_cast<std::uint32_t>(v);}
float value(std::uint32_t v) noexcept {return std::bit_cast<float>(v);}
using Words=std::array<std::uint32_t,3>;
Words words(const AffinePoint3&p) noexcept {return {bits(p.x),bits(p.y),bits(p.z)};}
AffinePoint3 point(const Words&p) noexcept {return {value(p[0]),value(p[1]),value(p[2])};}
bool bounded(std::uint32_t v,std::uint32_t maximum) noexcept {return (v&0x7fffffffu)<=maximum;}
bool rotated(std::uint32_t angle,const Words& vector,Words& output) noexcept {
    const auto sine=periodic_polynomial(angle),cosine=periodic_polynomial(sum(angle,0x3fc90fdbu));
    const Words a{0x3f800000u,0,0},b{0,0,0x3f800000u};Words first{},third{};
    for(unsigned row=0;row<3;++row){
        first[row]=sum(product(cosine,a[row]),product(sine,b[row]));
        third[row]=sum(product(cosine,b[row]),product(sine,a[row])^sign_bit);
    }
    AffineTransform3 matrix;matrix.columns={point(first),AffinePoint3{0,1,0},point(third)};
    AffinePoint3 result;if(!rounded_affine_transform(matrix,point(vector),result))return false;
    output=words(result);return true;
}
} // namespace

bool publish_main_balance_geometry(const MainBalanceGeometryInput& input,MainBalanceGeometryState& state) noexcept {
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::radix==2&&std::numeric_limits<float>::digits==24);
    std::array<std::uint32_t,2> longitudinal,lateral,balance;
    for(unsigned i=0;i<2;++i){
        longitudinal[i]=bits(input.longitudinal[i]);lateral[i]=bits(input.lateral[i]);balance[i]=bits(input.balance[i]);
        if(!bounded(longitudinal[i],0x47800000u)||!bounded(lateral[i],0x47800000u)||!bounded(balance[i],0x41800000u))return false;
    }
    const auto steering=bits(input.steering),orientation=bits(input.middle_orientation);
    if(!bounded(steering,0x41000000u)||!bounded(orientation,0x41000000u))return false;
    const auto old_first=words(state.first),old_second=words(state.second);
    for(auto v:old_first)if(!bounded(v,0x49800000u))return false;
    for(auto v:old_second)if(!bounded(v,0x49800000u))return false;
    if((longitudinal[0]&sign_bit)&&(longitudinal[0]&0x7fffffffu)){
        longitudinal[1]=sum(longitudinal[1],longitudinal[0]);longitudinal[0]=0;
    }
    // The first weighted contribution is stored as binary32 before subtracting
    // the second binary64 product. These distinct roundings are intentional.
    auto moment=narrow(multiply(multiply(single(balance[1]),constant(0x3fe51eb851eb851full)),single(longitudinal[1])));
    const auto other=multiply(multiply(single(balance[0]),constant(0x3fe451eb851eb852ull)),single(longitudinal[0]));
    moment=narrow(add(single(moment),negate(other)));
    Words first_rotation;
    if(!rotated(steering,{lateral[0],0,longitudinal[0]},first_rotation))return false;
    const auto difference=add(single(lateral[1]),negate(single(first_rotation[0])));
    moment=narrow(add(single(moment),multiply(difference,constant(0x3ff4000000000000ull))));
    const Words combined{sum(lateral[1],first_rotation[0]),first_rotation[1],sum(longitudinal[1],first_rotation[2])};
    Words world;if(!rotated(orientation,combined,world))return false;
    for(unsigned i=0;i<3;++i)world[i]=sum(world[i],old_first[i]);
    const Words delta{narrow(multiply(single(combined[2]),constant(0x3fc999999999999aull))),moment,
                      narrow(multiply(single(combined[0]),constant(0xbfc999999999999aull)))};
    Words second;for(unsigned i=0;i<3;++i)second[i]=sum(old_second[i],delta[i]);
    const MainBalanceGeometryState result{point(world),point(second),input.selected_index};
    std::memcpy(&state,&result,sizeof result);return true;
}
} // namespace ps2::native
