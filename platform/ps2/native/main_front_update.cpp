#include "main_front_update.h"
#include "contact_point_preparation.h"
#include "main_balance_math.h"
#include "recovered_atangent.h"
#include "recovered_scalar_metric.h"
#include "rounded_cross_product.h"
#include "vehicle_arithmetic.h"
#include <bit>

namespace ps2::native {
namespace {
namespace math=vehicle_math;
using math::Number;
float value(std::uint32_t bits) noexcept { return std::bit_cast<float>(bits); }
std::uint32_t bits(float v) noexcept { return std::bit_cast<std::uint32_t>(v); }
float negate(float v) noexcept { return value(bits(v)^0x80000000u); }
bool bounded(float v,std::uint32_t limit=0x49800000u) noexcept {return (bits(v)&0x7fffffffu)<=limit;}
bool bounded(AffinePoint3 v,std::uint32_t limit=0x49800000u) noexcept {
    return bounded(v.x,limit)&&bounded(v.y,limit)&&bounded(v.z,limit);
}
AffinePoint3 point(const std::array<float,3>& p) noexcept {return {p[0],p[1],p[2]};}
ContactVertex vertex(AffinePoint3 p) noexcept {return {p.x,p.y,p.z};}
struct Arithmetic {
    bool valid=true;
    Number n(float v) noexcept {auto r=math::from_single(v);valid&=r.valid;return r;}
    Number c(std::uint64_t v) noexcept {auto r=math::from_bits(v);valid&=r.valid;return r;}
    Number add(Number a,Number b) noexcept {auto r=math::add(a,b);valid&=r.valid;return r;}
    Number sub(Number a,Number b) noexcept {auto r=math::subtract(a,b);valid&=r.valid;return r;}
    Number mul(Number a,Number b) noexcept {auto r=math::multiply(a,b);valid&=r.valid;return r;}
    Number div(Number a,Number b) noexcept {auto r=math::divide(a,b);valid&=r.valid;return r;}
    float narrow(Number v) noexcept {auto r=math::narrow(v);valid&=math::finite(r);return r;}
    int compare(Number a,Number b) noexcept {int r=0;valid&=math::compare(a,b,r);return r;}
};
AffineTransform3 identity() noexcept {return {{{{1,0,0},{0,1,0},{0,0,1}}},{}};}
// Rotate the existing columns in the recovered order. Inverse orientation uses
// reverse axis order and negated input angles, not a transposed rounded matrix.
bool rotate(AffineTransform3& matrix,unsigned axis,float angle) noexcept {
    float sine,cosine;
    if(!main_balance_sine(angle,sine)||!main_balance_sine(math::add32(angle,value(0x3fc90fdb)),cosine))return false;
    const unsigned first=axis==0?1:0,second=axis==2?1:2;
    const auto a=matrix.columns[first],b=matrix.columns[second];
    const std::array<float,3> av{a.x,a.y,a.z},bv{b.x,b.y,b.z};
    std::array<float,3> x,y;
    for(unsigned i=0;i<3;++i){
        const auto ca=math::mul32(cosine,av[i]),sb=math::mul32(sine,bv[i]);
        const auto sa=math::mul32(sine,av[i]),cb=math::mul32(cosine,bv[i]);
        x[i]=math::add32(ca,axis==1?sb:negate(sb));
        y[i]=axis==1?math::sub32(cb,sa):math::add32(sa,cb);
    }
    matrix.columns[first]=point(x);matrix.columns[second]=point(y);return true;
}
bool absolute_points(AffinePoint3 orientation,AffinePoint3 position,
                     const std::array<AffinePoint3,4>& locals,std::array<AffinePoint3,4>& result) noexcept {
    ContactPointPreparation prepared;
    if(!prepare_contact_points(orientation,{},locals,prepared))return false;
    auto matrix=prepared.matrix;matrix.translation=position;
    for(unsigned i=0;i<4;++i)if(!rounded_affine_transform(matrix,locals[i],result[i]))return false;
    return true;
}
bool normalized(AffinePoint3 input,std::span<const ContactNormalPair> table,AffinePoint3& result) noexcept {
    if(!bounded(input)||table.size()!=8192)return false;
    const float squared=math::add32(math::add32(math::mul32(input.x,input.x),math::mul32(input.y,input.y)),math::mul32(input.z,input.z));
    const auto encoded=bits(squared);
    if(encoded>0x55800000u)return false;
    const auto& pair=table[((encoded>>11)&8191u)^4096u];
    if(bits(pair.slope)<0x3e000000u||bits(pair.slope)>0x3f800000u||bits(pair.intercept)<0x3f800000u||bits(pair.intercept)>0x40800000u)return false;
    const int exponent=63-int((encoded>>24)&127u);
    const auto scaled=[&](float v){return value(std::uint32_t(std::int64_t(bits(v))+std::int64_t(exponent)*0x800000));};
    const auto factor=math::sub32(scaled(pair.intercept),math::mul32(value((encoded&0x7fffffu)|0x3f800000u),scaled(pair.slope)));
    result={math::mul32(input.x,factor),math::mul32(input.y,factor),math::mul32(input.z,factor)};
    return math::finite(result.x)&&math::finite(result.y)&&math::finite(result.z);
}
float sum4(const std::array<AffinePoint3,4>& p,unsigned axis) noexcept {
    const auto component=[&](unsigned i){return axis==0?p[i].x:axis==1?p[i].y:p[i].z;};
    return math::add32(math::add32(math::add32(component(0),component(1)),component(2)),component(3));
}
}

bool update_main_front(const MainFrontInputs& input,MainFrontState& state,
                       MainFrontOutput& output,MainBalanceEffects& effects) noexcept {
    const auto in=input;auto next=state;MainFrontOutput result;MainBalanceEffects emitted;
    const auto& vehicle=next.contact.vehicle;
    const auto position=point(vehicle.position),orientation=point(vehicle.orientation),motion=point(vehicle.motion);
    if(!bounded(position)||!bounded(motion)||!bounded(orientation,0x41000000u)||
       !bounded(point(vehicle.candidate_position))||!bounded(point(vehicle.candidate_orientation),0x41000000u)||
       !bounded(in.steering,0x41000000u)||!bounded(in.object_parameter)||!bounded(in.control)||
       !bounded(next.contact.selected_scalar)||!bounded(next.angle_history))return false;
    Arithmetic a;
    std::array<AffinePoint3,4> predicted,current,delta,responses,local_responses;
    if(!absolute_points(point(vehicle.candidate_orientation),point(vehicle.candidate_position),in.local_points,predicted)||
       !absolute_points(orientation,position,in.local_points,current))return false;
    if(next.contact.contact_mask){
        for(unsigned i=0;i<4;++i){
            if(next.contact.contact_mask&1){ContactSearchResult found;
                if(!search_course_contact(in.course,in.segment,vertex(predicted[i]),found)||
                   !interpret_contact_result(found.data,vertex(predicted[i]),in.normal_table,next.contact.contacts[i]))return false;
            }
            next.contact.contact_mask=(next.contact.contact_mask>>1)|(next.contact.contact_mask&0x80000000u);
        }
    }
    for(unsigned i=0;i<4;++i)delta[i]={math::sub32(predicted[i].x,current[i].x),math::sub32(predicted[i].y,current[i].y),math::sub32(predicted[i].z,current[i].z)};
    AffinePoint3 unit_motion;
    if(!normalized(motion,in.normal_table,unit_motion))return false;
    auto direction_matrix=identity();
    if(!rotate(direction_matrix,1,orientation.y)||!rounded_affine_transform(direction_matrix,{0,0,1},result.forward))return false;
    float metric;
    // The recovered caller pairs these six values consecutively. This is
    // deliberately not the length of the supplied motion vector.
    if(!recovered_scalar_metric({0,0,motion.y}, {0,motion.x,motion.z},metric))return false;
    if(a.compare(a.n(metric),a.c(0x3f847ae147ae147bull))>0){
        CrossVector3 cross;
        if(!rounded_cross_product({result.forward.x,result.forward.y,result.forward.z},{unit_motion.x,unit_motion.y,unit_motion.z},cross))return false;
        float angle=0;
        if((bits(cross.y)&0x7fffffffu)<=0x3f800000u){
            const auto denominator=math::sqrt32(math::sub32(1,math::mul32(cross.y,cross.y)));
            if((bits(denominator)&0x7fffffffu)!=0){
                if(!recovered_atangent(math::div32(cross.y,denominator),angle))return false;
            }else angle=value((bits(cross.y)&0x80000000u)||!(bits(cross.y)&0x7fffffffu)?0xbfc90fdbu:0x3fc90fdbu);
        }
        if(a.compare(a.n(angle),a.c(0x3ff921fb54442d18ull))>0)angle=value(0x3fc90fdb);
        else if(a.compare(a.n(angle),a.c(0xbff921fb54442d18ull))<0)angle=value(0xbfc90fdb);
        next.angle_change=math::sub32(angle,next.angle_history);next.angle_history=angle;
    }else next.angle_history=next.angle_change=0;
    AffinePoint3 steering_direction;
    if(!rotate(direction_matrix,1,in.steering)||!rounded_affine_transform(direction_matrix,{0,0,1},steering_direction))return false;
    next.slots.type_mask=0;next.contact.vehicle.response_marker=0;
    for(unsigned i=0;i<4;++i){
        MainContactSlotInput slot;
        slot.slot=std::uint8_t(i);slot.contact=next.contact.contacts[i];slot.vector=vertex(delta[i]);
        slot.axis_direction=vertex(i<2?steering_direction:result.forward);slot.reference_middle=predicted[i].y;
        slot.object_parameter=in.object_parameter;slot.parameters=in.slot_parameters;slot.control=in.control;slot.normal_table=in.normal_table;
        MainContactSlotOutput response;
        if(!update_main_contact_slot(slot,next.slots,response))return false;
        responses[i]={response.response.x,response.response.y,response.response.z};
        if(response.active)next.contact.vehicle.response_marker|=1u<<i;
    }
    if(!(next.slots.type_mask&0x30u))next.sound_active=0;
    else if(!next.sound_active){emitted.commands[emitted.count++]={0x7b,0,false};next.sound_active=1;}
    bool kind_six=false;
    for(unsigned i=0;i<4;++i)kind_six|=((next.slots.packed_types>>(4*i))&15u)==6;
    if(!next.contact_event_active){
        if(kind_six&&!next.cabinet.enabled){next.cabinet.mode=2;next.cabinet.impulse=5;}
        else if(!kind_six&&next.cabinet.enabled)next.cabinet.mode=3;
    }
    next.contact_sums={math::add32(next.slots.lateral[0],next.slots.lateral[1]),math::add32(next.slots.metric[0],next.slots.metric[1]),
                       math::add32(next.slots.lateral[2],next.slots.lateral[3]),math::add32(next.slots.metric[2],next.slots.metric[3])};
    for(unsigned pair=0;pair<2;++pair)if((next.contact.vehicle.response_marker&(3u<<(2*pair)))==(3u<<(2*pair)))
        for(unsigned i=0;i<2;++i)next.contact_sums[2*pair+i]=a.narrow(a.mul(a.n(next.contact_sums[2*pair+i]),a.c(0x3fe0000000000000ull)));
    auto inverse=identity();
    if(!rotate(inverse,2,negate(orientation.z))||!rotate(inverse,0,negate(orientation.x))||!rotate(inverse,1,negate(orientation.y)))return false;
    for(unsigned i=0;i<4;++i)if(!rounded_affine_transform(inverse,responses[i],local_responses[i]))return false;
    result.force={math::add32(sum4(responses,0),0),math::add32(sum4(responses,1),0),math::add32(sum4(responses,2),0)};
    const auto sum_pair=[&](unsigned pair,unsigned axis){const auto& l=local_responses[2*pair];const auto& r=local_responses[2*pair+1];
        return math::add32(axis==0?l.x:axis==1?l.y:l.z,axis==0?r.x:axis==1?r.y:r.z);};
    const auto front_y=sum_pair(0,1),rear_y=sum_pair(1,1),front_x=sum_pair(0,0),rear_x=sum_pair(1,0);
    const auto moment_x=a.narrow(a.add(a.mul(a.sub(a.n(front_y),a.n(rear_y)),a.c(0x3ff599999999999aull)),a.n(0)));
    const auto moment_y=a.narrow(a.add(a.mul(a.sub(a.n(rear_x),a.n(front_x)),a.c(0x3ff599999999999aull)),a.n(0)));
    const auto weighted=[&](float front,float rear){return a.add(a.mul(a.n(front),a.c(0x3fe451eb851eb852ull)),a.mul(a.n(rear),a.c(0x3fe51eb851eb851full)));};
    const auto weighted_y=weighted(math::sub32(local_responses[0].y,local_responses[1].y),math::sub32(local_responses[2].y,local_responses[3].y));
    const auto weighted_z=weighted(math::sub32(local_responses[1].z,local_responses[0].z),math::sub32(local_responses[3].z,local_responses[2].z));
    const auto moment_z=a.narrow(a.add(weighted_y,a.n(0)));
    result.moment={a.narrow(a.add(a.mul(a.n(sum4(local_responses,2)),a.c(0x3fc999999999999aull)),a.n(moment_x))),
                   a.narrow(a.add(a.n(moment_y),weighted_z)),
                   a.narrow(a.sub(a.n(moment_z),a.mul(a.n(sum4(local_responses,0)),a.c(0x3fc999999999999aull))))};
    if((bits(next.contact.selected_scalar)&0x7fffffffu)!=0){
        AffinePoint3 local_motion;if(!rounded_affine_transform(inverse,motion,local_motion))return false;
        const auto correction=[&](std::uint64_t scale,float component){return a.mul(a.mul(a.mul(a.mul(a.n(next.contact.selected_scalar),a.c(scale)),a.n(in.object_parameter)),a.n(component)),a.c(0x3fc999999999999aull));};
        result.moment.x=a.narrow(a.sub(a.n(result.moment.x),correction(0x3fb999999999999aull,local_motion.z)));
        result.moment.z=a.narrow(a.add(a.n(result.moment.z),correction(0x3fe0000000000000ull,local_motion.x)));
    }
    result.speed_metric=metric;
    if(in.longitudinal_metric){AffinePoint3 local_motion;auto middle_inverse=identity();
        if(!rotate(middle_inverse,1,negate(orientation.y))||!rounded_affine_transform(middle_inverse,motion,local_motion))return false;
        // The original comparison preserves negative zero.
        const auto raw=bits(local_motion.z);
        result.speed_metric=(raw&0x80000000u)&&(raw&0x7fffffffu)?negate(local_motion.z):local_motion.z;
    }else if(in.scene_metric_scale){float sine;
        if(!main_balance_sine(a.narrow(a.div(a.c(0x3f66f60478722053ull),a.n(in.control))),sine))return false;
        result.speed_metric=math::mul32(metric,sine);
    }
    result.incoming_moment=a.narrow(a.mul(a.n(math::mul32(math::sub32(next.slots.published_middle[1],next.slots.published_middle[0]),in.object_parameter)),a.c(0x3f747ae147ae147bull)));
    if(!a.valid||!bounded(result.force)||!bounded(result.moment)||!bounded(result.forward)||!bounded(result.speed_metric)||!bounded(result.incoming_moment))return false;
    state=next;output=result;effects=emitted;return true;
}
} // namespace ps2::native
