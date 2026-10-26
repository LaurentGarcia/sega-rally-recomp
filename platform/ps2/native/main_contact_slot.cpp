#include "main_contact_slot.h"
#include "recovered_scalar_metric.h"
#include <array>
#include <bit>
#include <cstdint>
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
std::uint32_t bits(float v) noexcept{return std::bit_cast<std::uint32_t>(v);}
float value(std::uint32_t v) noexcept{return std::bit_cast<float>(v);}
bool bounded(std::uint32_t word,std::uint32_t limit) noexcept{return (word&magnitude_mask)<=limit;}
bool positive(std::uint32_t word) noexcept{return !(word&sign_bit)&&(word&magnitude_mask);}
bool negative(std::uint32_t word) noexcept{return (word&sign_bit)&&(word&magnitude_mask);}
std::uint32_t scale(std::uint32_t word,int power) noexcept{return (word&0x7fffffu)|(std::uint32_t(int((word>>23)&255)+power)<<23);}
bool normalized_xz(std::uint32_t x,std::uint32_t z,std::span<const ContactNormalPair> table,std::array<std::uint32_t,2>& out) noexcept {
    if(table.size()!=8192)return false;
    const auto squared=sum(sum(product(x,x),0),product(z,z));
    if((squared&&squared<0x2b800000u)||squared>0x54800000u)return false;
    const auto row=((squared>>11)&8191u)^4096u;
    const auto slope=bits(table[row].slope),intercept=bits(table[row].intercept);
    if(slope<0x3e000000u||slope>0x3f800000u||intercept<0x3f800000u||intercept>0x40800000u)return false;
    const int power=63-int((squared>>24)&127u);
    const auto mantissa=(squared&0x7fffffu)|0x3f800000u;
    const auto factor=sum(scale(intercept,power),product(mantissa,scale(slope,power))^sign_bit);
    out={product(x,factor),product(z,factor)};return true;
}
} // namespace
bool update_main_contact_slot(const MainContactSlotInput& input,MainContactSlotState& state,MainContactSlotOutput& output) noexcept {
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::radix==2&&std::numeric_limits<float>::digits==24);
    if(input.slot>=4)return false;
    const unsigned slot=input.slot;const auto gap=bits(input.contact.gap),middle=bits(input.reference_middle);
    if(!bounded(gap,0x41800000u)||!bounded(middle,0x49800000u))return false;
    MainContactSlotState result;std::memcpy(&result,&state,sizeof result);MainContactSlotOutput response;
    const unsigned kind=input.contact.status&15u;
    result.packed_types=std::uint16_t((result.packed_types&~(15u<<(4*slot)))|(kind<<(4*slot)));
    result.type_mask|=1u<<kind;result.published_middle[slot]=value(sum(middle,gap));
    if((input.contact.status&128u)||negative(gap)){
        const auto sibling=bits(state.history[slot^1u]);if(!bounded(sibling,0x41800000u))return false;
        result.history[slot]=0;result.gain[slot]=0;result.metric[slot]=0;result.lateral[slot]=0;
        if(positive(sibling)){
            const auto factor=bits(input.factor),parameter=bits(input.object_parameter),direction=bits(input.contact.direction[1]);
            if(!bounded(factor,0x41800000u)||!bounded(parameter,0x47800000u)||!bounded(direction,0x41800000u))return false;
            auto rebound=multiply(single(gap),constant(0x3f789374bc6a7efaull));
            rebound=multiply(rebound,single(factor));rebound=multiply(rebound,single(parameter));rebound=multiply(rebound,single(direction));
            response.response.y=value(narrow(rebound));
        }
    }else{
        const auto stiffness=bits(input.parameters[slot<2?1:2]),damping=bits(input.parameters[slot<2?3:4]);
        const auto control=bits(input.control),history=bits(state.history[slot]);
        if(!bounded(stiffness,0x47800000u)||!bounded(damping,0x47800000u)||!bounded(control,0x41800000u)||!bounded(history,0x41800000u))return false;
        std::array<std::uint32_t,3> direction;for(unsigned i=0;i<3;++i){direction[i]=bits(input.contact.direction[i]);if(!bounded(direction[i],0x41800000u))return false;}
        auto magnitude=narrow(multiply(multiply(single(product(stiffness,gap)),constant(0x4059000000000000ull)),single(control)));
        auto retained_gap=gap;
        // The strict comparison is against binary64(0.2); the subsequent
        // retained gap is its separately rounded binary32 value.
        if(positive(gap)&&gap>=0x3e4ccccdu){
            const auto factor=bits(input.factor),parameter=bits(input.object_parameter);
            if(!bounded(factor,0x41800000u)||!bounded(parameter,0x47800000u))return false;
            auto excess=add(single(gap),negate(constant(0x3fc999999999999aull)));
            excess=multiply(excess,single(factor));excess=multiply(excess,single(parameter));
            magnitude=narrow(add(single(magnitude),excess));retained_gap=0x3e4ccccdu;
        }
        const auto change=product(product(sum(history,retained_gap^sign_bit),damping),control);
        magnitude=narrow(add(single(magnitude),negate(multiply(single(change),constant(0x40ac200000000000ull)))));
        if(negative(magnitude))magnitude=0;
        result.history[slot]=value(retained_gap);result.gain[slot]=value(magnitude);
        response.response={value(product(magnitude,direction[0])),value(product(magnitude,direction[1])),value(product(magnitude,direction[2]))};
        const auto x=bits(input.vector.x),y=bits(input.vector.y),z=bits(input.vector.z);
        if(!bounded(x,0x49800000u)||!bounded(y,0x49800000u)||!bounded(z,0x49800000u))return false;
        float metric;if(!recovered_scalar_metric({0,0,input.vector.y},{0,input.vector.x,input.vector.z},metric))return false;
        // Largest binary32 value below binary64(1e-10); equality stays below.
        if(bits(metric)<=0x2edbe6feu){result.metric[slot]=0;result.lateral[slot]=0;}
        else{
            const auto ax=bits(input.axis_direction.x),az=bits(input.axis_direction.z);
            if(!bounded(ax,0x41800000u)||!bounded(az,0x41800000u))return false;
            std::array<std::uint32_t,2> normalized;if(!normalized_xz(x,z,input.normal_table,normalized))return false;
            const auto cross=sum(product(normalized[0],az),product(normalized[1],ax)^sign_bit);
            auto cosine=sum(product(normalized[0],ax),product(normalized[1],az))&magnitude_mask;
            if(cosine>0x3f800000u)cosine=0x3f800000u;
            const auto sine=root(sum(0x3f800000u,product(cosine,cosine)^sign_bit));
            result.lateral[slot]=value(product(negative(cross)?0xbf800000u:0x3f800000u,sine));
            result.metric[slot]=metric;
        }
        response.active=true;
    }
    std::memcpy(&state,&result,sizeof result);std::memcpy(&output,&response,sizeof response);return true;
}
} // namespace ps2::native
