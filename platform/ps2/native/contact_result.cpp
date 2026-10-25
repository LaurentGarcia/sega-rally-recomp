#include "contact_result.h"
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
std::uint32_t difference(std::uint32_t left,std::uint32_t right) noexcept { return sum(left,right^sign_bit); }
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
    // The divisor domain is [2^-20,2^20]. The scaled numerator therefore
    // needs at most48 bits, including subnormal numerators and underflow.
    const auto numerator=std::uint64_t(a.numerator)<<shift;
    auto significand=numerator/b.numerator;
    const auto remainder=numerator%b.numerator;
    const auto twice=remainder*2;
    if(twice>b.numerator||(twice==b.numerator&&(significand&1)))++significand;
    return rounded(significand,quantum,sign);
}
std::uint32_t bits(float value) noexcept { return std::bit_cast<std::uint32_t>(value); }
bool bounded(std::uint32_t value) noexcept { return (value&magnitude_mask)<=0x49800000u; }
bool divisor(std::uint32_t value) noexcept {
    const auto magnitude=value&magnitude_mask;
    return magnitude>=0x35800000u&&magnitude<=0x49800000u;
}
std::uint32_t scale(std::uint32_t value,int power) noexcept {
    // The validated pair and squared-length domains keep this exponent in
    // 103..149: no subnormal, wrapping, overflow or hardware-port behavior.
    const auto exponent=int((value>>23)&255u)+power;
    return (value&0x7fffffu)|(std::uint32_t(exponent)<<23);
}
bool normal(const std::array<std::uint32_t,3>& direction,
            std::span<const ContactNormalPair> table,std::array<float,3>& output) noexcept {
    if(table.size()!=8192)return false;
    const auto squared=sum(sum(product(direction[0],direction[0]),product(direction[1],direction[1])),product(direction[2],direction[2]));
    if(squared<0x2b800000u||squared>0x54800000u)return false; // 2^-40..2^42.
    const auto row=((squared>>11)&8191u)^4096u;
    const auto slope=bits(table[row].slope),intercept=bits(table[row].intercept);
    if(slope<0x3e000000u||slope>0x3f800000u||intercept<0x3f800000u||intercept>0x40800000u)return false;
    const int power=63-int((squared>>24)&127u);
    const auto mantissa=(squared&0x7fffffu)|0x3f800000u;
    const auto factor=difference(scale(intercept,power),product(mantissa,scale(slope,power)));
    for(unsigned i=0;i<3;++i)output[i]=std::bit_cast<float>(product(direction[i],factor));
    return true;
}
} // namespace

bool interpret_contact_result(const ContactPolygon& polygon,const ContactVertex& point,
    std::span<const ContactNormalPair> table,ContactResult& output) noexcept {
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::digits==24);
    ContactResult result;
    if(!polygon.attributes){
        std::memcpy(&result,&output,sizeof result);result.status=15;
        std::memcpy(&output,&result,sizeof result);return true;
    }
    result.status=(polygon.attributes>>16)&0x8fu;
    const auto x=bits(point.x),z=bits(point.z);
    if(!bounded(x)||!bounded(z))return false;
    const auto& coefficients=polygon.coefficients;
    std::uint32_t gap;
    if(result.status&0x80u){
        const auto ax=bits(polygon.vertices[0].x),az=bits(polygon.vertices[0].z);
        const auto bx=bits(polygon.vertices[1].x),bz=bits(polygon.vertices[1].z),denominator=bits(coefficients[3]);
        if(!bounded(ax)||!bounded(az)||!bounded(bx)||!bounded(bz)||!divisor(denominator))return false;
        const auto determinant=difference(product(difference(ax,x),difference(bz,z)),product(difference(az,z),difference(bx,x)));
        gap=quotient(determinant&magnitude_mask,denominator);
        std::memcpy(result.direction.data(),coefficients.data(),3*sizeof(float));
    }else{
        const auto y=bits(point.y),nx=bits(coefficients[0]),ny=bits(coefficients[1]),nz=bits(coefficients[2]),d=bits(coefficients[3]);
        if(!bounded(y)||!bounded(nx)||!bounded(nz)||!bounded(d)||!divisor(ny))return false;
        const auto numerator=difference(difference(product(nx^sign_bit,x),product(nz,z)),d);
        gap=difference(quotient(numerator,ny),y);
        if(!normal({nx,ny,nz},table,result.direction))return false;
    }
    result.gap=std::bit_cast<float>(gap);
    std::memcpy(&output,&result,sizeof result);
    return true;
}
} // namespace ps2::native
