#include "car_render_bridge.h"
#include "main_balance_math.h"
#include "vehicle_arithmetic.h"
#include <bit>

namespace ps2::native {
namespace {
namespace math=vehicle_math;
float value(std::uint32_t bits) noexcept{return std::bit_cast<float>(bits);}
float negative(float v) noexcept{return value(std::bit_cast<std::uint32_t>(v)^0x80000000u);}
bool bounded(float v,std::uint32_t limit) noexcept{return (std::bit_cast<std::uint32_t>(v)&0x7fffffffu)<=limit;}
bool bounded(AffinePoint3 p,std::uint32_t limit) noexcept{return bounded(p.x,limit)&&bounded(p.y,limit)&&bounded(p.z,limit);}
AffinePoint3 point(const std::array<std::uint32_t,3>& words) noexcept{return {value(words[0]),value(words[1]),value(words[2])};}
bool valid_matrix(const AffineTransform3& matrix) noexcept {
    for(const auto& c:matrix.columns)if(!bounded(c,0x41800000u))return false;
    return bounded(matrix.translation,0x49800000u);
}
// The wheel phase is an accumulated angle. Reuse the established polynomial
// on its small domain; extend its same reduction for the bounded render phase.
// |angle| <= 2^20+2 keeps the integer quotient below 2^19, exactly representable
// in binary32, and leaves the centered polynomial argument bounded by 2.
bool render_sine(float angle,float& output) noexcept {
    const auto bits=std::bit_cast<std::uint32_t>(angle),magnitude=bits&0x7fffffffu;
    if(magnitude<=0x41800000u)return main_balance_sine(angle,output);
    if(magnitude>0x49800010u)return false;
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
    if((unsigned(whole)+(bits>>31))&1u)result=negative(result);
    if(!math::finite(result))return false;
    output=result;return true;
}
// Existing project-authored polynomial/value arithmetic; each product and sum
// has its own binary32 boundary. No host trigonometric approximation is used.
bool rotate(AffineTransform3& matrix,unsigned axis,float angle) noexcept {
    float sine,cosine;
    if(!render_sine(angle,sine)||!render_sine(math::add32(angle,value(0x3fc90fdb)),cosine))return false;
    const unsigned first=axis==0?1:0,second=axis==2?1:2;
    const auto a=matrix.columns[first],b=matrix.columns[second];
    const std::array<float,3> av{a.x,a.y,a.z},bv{b.x,b.y,b.z};
    std::array<float,3> x{},y{};
    for(unsigned i=0;i<3;++i){
        const auto ca=math::mul32(cosine,av[i]),sb=math::mul32(sine,bv[i]);
        const auto sa=math::mul32(sine,av[i]),cb=math::mul32(cosine,bv[i]);
        x[i]=math::add32(ca,axis==1?sb:negative(sb));
        y[i]=axis==1?math::sub32(cb,sa):math::add32(sa,cb);
    }
    matrix.columns[first]={x[0],x[1],x[2]};matrix.columns[second]={y[0],y[1],y[2]};return true;
}
bool translate(AffineTransform3& matrix,AffinePoint3 offset) noexcept {
    return rounded_affine_transform(matrix,offset,matrix.translation);
}
float scaled(float input,std::uint64_t coefficient) noexcept {
    return math::narrow(math::multiply(math::from_single(input),math::from_bits(coefficient)));
}
}
bool build_car_render_bridge(const RenderPose& pose,const CarRenderGeometry& geometry,
                             const AffineTransform3& view,CarRenderBridge& output) noexcept {
    const auto position=point(pose.words.position),orientation=point(pose.words.orientation);
    const auto steering=value(pose.words.attributes[0]),spin=value(pose.words.attributes[2]);
    std::array<float,4> suspension{},heights{};
    for(unsigned i=0;i<4;++i){
        suspension[i]=value(pose.words.attributes[3+i]);heights[i]=value(pose.words.attributes[7+i]);
        if(!bounded(suspension[i],0x42800000u)||!bounded(heights[i],0x49800000u)||!bounded(geometry.wheel_offsets[i],0x42800000u))return false;
    }
    if(!bounded(position,0x49800000u)||!bounded(orientation,0x41000000u)||
       !bounded(steering,0x41000000u)||!bounded(spin,0x49800000u)||!valid_matrix(view))return false;
    CarRenderBridge result;
    result.body=view;
    if(!translate(result.body,position)||!rotate(result.body,1,orientation.y)||
       !rotate(result.body,0,orientation.x)||!rotate(result.body,2,orientation.z))return false;
    for(unsigned i=0;i<4;++i){
        auto offset=geometry.wheel_offsets[i];offset.y=math::add32(offset.y,suspension[i]);
        result.wheels[i]=result.body;
        if(!translate(result.wheels[i],offset)||(i<2&&!rotate(result.wheels[i],1,steering))||
           !rotate(result.wheels[i],0,negative(spin)))return false;
    }
    const auto mean=scaled(math::add32(math::add32(math::add32(heights[0],heights[1]),heights[2]),heights[3]),0x3fd0000000000000ull);
    const auto shadow_y=math::narrow(math::subtract(math::from_single(mean),math::from_bits(0x3f947ae147ae147bull)));
    const auto pitch=scaled(math::sub32(math::add32(heights[0],heights[1]),math::add32(heights[2],heights[3])),0x3fce76c8b4395810ull);
    const auto roll=scaled(math::sub32(math::add32(heights[0],heights[2]),math::add32(heights[1],heights[3])),0x3fd54fdf3b645a1dull);
    result.shadow=view;
    if(!translate(result.shadow,{position.x,shadow_y,position.z})||!rotate(result.shadow,1,orientation.y)||
       !rotate(result.shadow,0,pitch)||!rotate(result.shadow,2,roll))return false;
    if(!valid_matrix(result.body)||!valid_matrix(result.shadow))return false;
    for(const auto& wheel:result.wheels)if(!valid_matrix(wheel))return false;
    output=result;return true;
}
}
