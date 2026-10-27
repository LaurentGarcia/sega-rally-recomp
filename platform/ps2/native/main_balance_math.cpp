#include "main_balance_math.h"
#include "vehicle_arithmetic.h"
#include <array>
#include <bit>

namespace ps2::native {
namespace {
namespace math=vehicle_math;
float value(std::uint32_t bits) noexcept {return std::bit_cast<float>(bits);}
float negative(float x) noexcept {return value(std::bit_cast<std::uint32_t>(x)^0x80000000u);}
}
bool main_balance_sine(float angle,float& output) noexcept {
    const auto bits=std::bit_cast<std::uint32_t>(angle),magnitude=bits&0x7fffffffu;
    if(magnitude>0x41800000u)return false;
    std::int32_t whole;
    if(!math::truncate(math::from_single(math::mul32(value(magnitude),value(0x3ea2f983))),whole))return false;
    const auto multiple=math::mul32(value(0x40490fdb),math::narrow(math::from_integer(whole)));
    const auto centered=math::sub32(math::sub32(value(magnitude),multiple),value(0x3fc90fdb));
    const auto square=math::mul32(centered,centered);
    std::array<float,6> powers{square};
    constexpr std::array<std::uint32_t,6> coefficient{0x3f000000u,0x3d2aaaabu,0x3ab60b61u,0x37d00d01u,0x3493f27eu,0x310f76c7u};
    for(unsigned i=1;i<6;++i)powers[i]=math::mul32(square,powers[i-1]);
    auto result=math::mul32(powers[5],value(coefficient[5]));
    for(int i=4;i>=0;--i){
        auto term=math::mul32(powers[unsigned(i)],value(coefficient[unsigned(i)]));
        if(!(i&1))term=negative(term);
        result=math::add32(result,term);
    }
    result=math::add32(result,1);
    const unsigned nonpositive=(bits>>31)||!magnitude;
    if((unsigned(whole)+nonpositive)&1u)result=negative(result);
    if(!math::finite(result))return false;
    output=result;return true;
}
bool main_balance_rotate(float angle,const AffinePoint3& input,AffinePoint3& output) noexcept {
    float sine,cosine;
    if(!main_balance_sine(angle,sine)||!main_balance_sine(math::add32(angle,value(0x3fc90fdb)),cosine))return false;
    const std::array<float,3> a{1,0,0},b{0,0,1};
    std::array<float,3> first{},third{};
    for(unsigned i=0;i<3;++i){
        first[i]=math::add32(math::mul32(cosine,a[i]),math::mul32(sine,b[i]));
        third[i]=math::sub32(math::mul32(cosine,b[i]),math::mul32(sine,a[i]));
    }
    AffineTransform3 matrix;
    matrix.columns={AffinePoint3{first[0],first[1],first[2]},AffinePoint3{0,1,0},AffinePoint3{third[0],third[1],third[2]}};
    return rounded_affine_transform(matrix,input,output);
}
} // namespace ps2::native
