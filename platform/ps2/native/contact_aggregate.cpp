#include "contact_aggregate.h"
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
    // needs at most 48 bits, including subnormal numerators and underflow.
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
// Binary32 times these small integers is exact in binary64 (at most 37
// significant bits). Truncate the exact dyadic product without host FP.
bool truncated_product(std::uint32_t value, unsigned multiplier, std::int32_t& output) noexcept {
    const auto a=parts(value);
    std::uint64_t magnitude=std::uint64_t(a.numerator)*multiplier;
    if(a.power>=0){
        if(a.power>=32 || magnitude>(std::uint64_t(0x80000000u)>>a.power))return false;
        magnitude<<=a.power;
    }else magnitude=(-a.power>=64)?0:(magnitude>>-a.power);
    const bool negative=(value&sign_bit)!=0;
    if(magnitude>(negative?0x80000000ull:0x7fffffffull))return false;
    output=negative?(magnitude==0x80000000ull?std::numeric_limits<std::int32_t>::min():-std::int32_t(magnitude)):std::int32_t(magnitude);
    return true;
}
} // namespace

bool accumulate_contact_scalar(float accumulator,float metric,float sign_source,float& output) noexcept {
    const auto a=bits(accumulator),m=bits(metric),s=bits(sign_source);
    if(!bounded(a)||!bounded(m)||!bounded(s)||((m&sign_bit)&&(m&magnitude_mask)))return false;
    // Comparing either signed zero with zero takes the nonnegative branch.
    const auto signed_metric=((s&sign_bit)&&(s&magnitude_mask))?(m^sign_bit):m;
    // Two binary32 operands cannot produce a binary64 rounding error that
    // changes their subsequent nearest-even binary32 sum in this domain.
    const auto result=difference(a,signed_metric);
    output=std::bit_cast<float>(result);
    return true;
}

bool apply_contact_aggregate(const ContactAggregateInput& input,ContactAggregateState& state) noexcept {
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::digits==24);
    const auto scalar=bits(input.signed_scalar),linear=bits(input.response_divisor),angular=bits(input.scalar_divisor);
    if(!bounded(scalar)||!divisor(linear)||!divisor(angular))return false;
    for(unsigned i=0;i<3;++i)if(!bounded(bits(input.bases[i]))||!bounded(bits(state.motion[i])))return false;
    for(const auto& response:input.responses)if(!bounded(bits(response[0]))||!bounded(bits(response[2])))return false;
    ContactAggregateState result;
    std::memcpy(&result,&state,sizeof result);
    const auto scaled=quotient(scalar,angular);
    const auto middle=sum(bits(state.motion[2]),scaled);
    const auto sum_first=sum(sum(sum(bits(input.responses[0][0]),bits(input.responses[1][0])),bits(input.responses[2][0])),bits(input.responses[3][0]));
    const auto sum_third=sum(sum(sum(bits(input.responses[0][2]),bits(input.responses[1][2])),bits(input.responses[2][2])),bits(input.responses[3][2]));
    const auto first=sum(quotient(sum_first,linear),bits(state.motion[0]));
    const auto third=sum(quotient(sum_third,linear),bits(state.motion[1]));
    result.motion={std::bit_cast<float>(first),std::bit_cast<float>(third),std::bit_cast<float>(middle)};
    result.candidate_position[0]=std::bit_cast<float>(sum(bits(input.bases[0]),first));
    result.candidate_position[2]=std::bit_cast<float>(sum(bits(input.bases[1]),third));
    result.candidate_orientation[1]=std::bit_cast<float>(sum(bits(input.bases[2]),middle));
    if(input.feedback_enabled){
        std::int32_t magnitude,signed_feedback;
        if(!truncated_product(scaled,1000,magnitude)||magnitude==std::numeric_limits<std::int32_t>::min()||
           !truncated_product(scaled,8000,signed_feedback))return false;
        result.feedback={magnitude<0?-magnitude:magnitude,signed_feedback};
    }
    std::memcpy(&state,&result,sizeof result);
    return true;
}
} // namespace ps2::native
