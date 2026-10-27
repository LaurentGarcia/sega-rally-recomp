#include "recovered_atangent.h"
#include "vehicle_arithmetic.h"
#include <bit>
#include <cstdint>

namespace ps2::native {
bool recovered_atangent(float argument,float& output) noexcept {
    namespace math=vehicle_math;
    const auto bits=std::bit_cast<std::uint32_t>(argument),magnitude=bits&0x7fffffffu;
    if(magnitude>=0x7f800000u)return false;
    // At this bound the omitted cubic is smaller than half the narrower
    // neighboring binary32 spacing, including the power-of-two boundary.
    if(magnitude<=0x39000000u){output=argument;return true;}
    const auto one=math::from_integer(1);
    auto x=math::from_single(std::bit_cast<float>(magnitude));
    const bool reciprocal=magnitude>0x3f800000u;
    if(reciprocal)x=math::divide(one,x);
    int order=0;
    if(!math::compare(x,math::from_bits(0x3fe0000000000000ull),order))return false;
    const bool shifted=order>0;
    if(shifted)x=math::divide(math::subtract(x,one),math::add(x,one));
    // |x|<=1/2. Forty odd terms bound the exact-series tail below2^-80.
    // Each intervening operation retains its own binary64 rounding boundary.
    const auto squared=math::multiply(x,x);
    auto term=x,sum=x;
    for(int k=1;k<40;++k){
        term=math::multiply(term,squared);
        const auto part=math::divide(term,math::from_integer(2*k+1));
        sum=(k&1)?math::subtract(sum,part):math::add(sum,part);
        if(!sum.valid||!part.valid)return false;
        if(!part.significand||part.power+60<sum.power)break;
    }
    if(shifted)sum=math::add(math::from_bits(0x3fe921fb54442d18ull),sum);
    if(reciprocal)sum=math::subtract(math::from_bits(0x3ff921fb54442d18ull),sum);
    if(bits>>31)sum=math::negate(sum);
    const auto result=math::narrow(sum);
    if(!math::finite(result))return false;
    output=result;return true;
}
} // namespace ps2::native
