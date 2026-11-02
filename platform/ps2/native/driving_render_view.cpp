#include "driving_render_view.h"
#include "angle_fold.h"
#include "main_balance_math.h"
#include "vehicle_arithmetic.h"
#include <array>
#include <bit>

namespace ps2::native {
namespace {
namespace math=vehicle_math;
float value(std::uint32_t bits) noexcept{return std::bit_cast<float>(bits);}
float negative(float v) noexcept{return value(std::bit_cast<std::uint32_t>(v)^0x80000000u);}
bool bounded(float v,std::uint32_t maximum) noexcept{return (std::bit_cast<std::uint32_t>(v)&0x7fffffffu)<=maximum;}
bool bounded(AffinePoint3 p,std::uint32_t maximum) noexcept{return bounded(p.x,maximum)&&bounded(p.y,maximum)&&bounded(p.z,maximum);}
AffinePoint3 point(const std::array<std::uint32_t,3>&v) noexcept{return {value(v[0]),value(v[1]),value(v[2])};}
bool rotate(AffineTransform3& matrix,unsigned axis,float angle) noexcept {
    float sine,cosine;
    if(!main_balance_sine(angle,sine)||!main_balance_sine(math::add32(angle,value(0x3fc90fdb)),cosine))return false;
    const unsigned first=axis==0?1:0,second=2;
    const auto a=matrix.columns[first],b=matrix.columns[second];
    const std::array<float,3> av{a.x,a.y,a.z},bv{b.x,b.y,b.z};std::array<float,3>x{},y{};
    for(unsigned i=0;i<3;++i){
        const auto ca=math::mul32(cosine,av[i]),sb=math::mul32(sine,bv[i]);
        const auto sa=math::mul32(sine,av[i]),cb=math::mul32(cosine,bv[i]);
        x[i]=math::add32(ca,axis==1?sb:negative(sb));
        y[i]=axis==1?math::sub32(cb,sa):math::add32(sa,cb);
    }
    matrix.columns[first]={x[0],x[1],x[2]};matrix.columns[second]={y[0],y[1],y[2]};return true;
}
}
bool update_driving_render_view(const RenderPose& pose,const DrivingRenderViewConfig& config,
                                DrivingRenderViewState& state,DrivingRenderView& output) noexcept {
    const auto position=point(pose.words.position),orientation=point(pose.words.orientation);
    const float previous=state.yaw_history,adjustment=config.distance_adjustment;
    if(!bounded(position,0x49800000u)||!bounded(orientation,0x40490fdbu)||
       !bounded(previous,0x40490fdbu)||!bounded(adjustment,0x42800000u))return false;
    auto difference=math::add32(orientation.y,previous);
    if(!fold_angle_once(difference))return false;
    const auto correction=math::multiply(math::from_single(difference),math::from_bits(0x3fa999999999999aull));
    auto yaw=math::narrow(math::subtract(math::from_single(previous),correction));
    if(!fold_angle_once(yaw))return false;
    const auto half=math::multiply(math::from_single(orientation.x),math::from_bits(0x3fe0000000000000ull));
    const auto pitch=math::narrow(math::subtract(math::from_bits(0x3fb1df46a2525ff7ull),half));
    const auto distance=math::narrow(math::add(math::from_single(adjustment),math::from_bits(0x4014000000000000ull)));
    if(!math::finite(pitch)||!math::finite(distance))return false;
    DrivingRenderView result;
    result.world_to_view.columns={AffinePoint3{1,0,0},AffinePoint3{0,1,0},AffinePoint3{0,0,1}};
    if(!rounded_affine_transform(result.world_to_view,{0,value(0xbfa66666u),distance},result.world_to_view.translation)||
       !rotate(result.world_to_view,0,pitch)||!rotate(result.world_to_view,1,yaw)||
       !rounded_affine_transform(result.world_to_view,{negative(position.x),negative(position.y),negative(position.z)},result.world_to_view.translation))return false;
    // The source's inverse helper transposes the rounded rotation, then applies
    // the negative translation using the same rounded affine operation.
    const auto& m=result.world_to_view;AffineTransform3 inverse;
    inverse.columns={AffinePoint3{m.columns[0].x,m.columns[1].x,m.columns[2].x},
                     AffinePoint3{m.columns[0].y,m.columns[1].y,m.columns[2].y},
                     AffinePoint3{m.columns[0].z,m.columns[1].z,m.columns[2].z}};
    if(!rounded_affine_transform(inverse,{negative(m.translation.x),negative(m.translation.y),negative(m.translation.z)},inverse.translation)||
       !rounded_affine_transform(inverse,{},result.position))return false;
    result.orientation=orientation;
    state.yaw_history=yaw;output=result;return true;
}
}
