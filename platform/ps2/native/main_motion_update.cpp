#include "main_motion_update.h"
#include "angle_fold.h"
#include "recovered_scalar_metric.h"
#include "rounded_cross_product.h"
#include "vehicle_arithmetic.h"
#include <bit>
#include <cstring>

namespace ps2::native {
namespace {
namespace math=vehicle_math;
using math::from_single;using math::from_bits;using math::narrow;
std::uint32_t bits(float value) noexcept {return std::bit_cast<std::uint32_t>(value);}
float value(std::uint32_t word) noexcept {return std::bit_cast<float>(word);}
bool bounded(float v) noexcept {return (bits(v)&0x7fffffffu)<=0x49800000u;}
bool positive(float v) noexcept {return !(bits(v)&0x80000000u)&&(bits(v)&0x7fffffffu);}
bool negative(float v) noexcept {return (bits(v)&0x80000000u)&&(bits(v)&0x7fffffffu);}
std::array<float,3> words(AffinePoint3 p) noexcept {return {p.x,p.y,p.z};}
bool greater(float a,std::uint64_t constant) noexcept {
    int order=0;return math::compare(from_single(a),from_bits(constant),order)&&order>0;
}
float signed_clamp(float v) noexcept {
    const auto word=bits(v);
    return (word&0x7fffffffu)>=0x3dcccccdu?value((word&0x80000000u)|0x3dcccccdu):v;
}
bool append(MainBalanceEffects& out,std::uint32_t code,std::uint32_t parameter,bool has_parameter) noexcept {
    if(out.count>=out.commands.size())return false;
    out.commands[out.count++]={code,parameter,has_parameter};return true;
}
std::uint32_t random_next(std::uint32_t& seed) noexcept {
    const auto product=std::uint64_t(seed)*0x5d588b65u;
    seed=(std::uint32_t(product)+2u*std::uint32_t(product>>32)+std::uint32_t((product>>31)&1u))&0x7fffffffu;
    return seed;
}
float shaped(float angle,float coefficient,std::uint64_t scale) noexcept {
    auto factor=math::multiply(from_single(coefficient),from_bits(scale));
    if(negative(angle))factor=math::negate(factor);
    return narrow(math::subtract(from_single(angle),
        math::multiply(factor,from_single(math::mul32(angle,angle)))));
}
bool contact_events(const MainMotionInput& input,MainMotionState& state,MainBalanceEffects& events) noexcept {
    if(negative(input.selected_scalar)||input.mode==2){state.quiet_updates=0;state.contact_event_active=0;return true;}
    if(!(bits(input.selected_scalar)&0x7fffffffu)){
        ++state.quiet_updates;
        if(state.contact_event_active){state.contact_event_active=0;state.cabinet.mode=3;}
        return true;
    }
    state.diagnostic_marker={2,2,46};
    if(std::bit_cast<std::int32_t>(state.quiet_updates)>30){
        if(!state.cabinet.mode){state.cabinet.mode=1;state.cabinet.countdown=state.feedback[0];state.cabinet.impulse=state.feedback[1];}
        if(greater(input.selected_scalar,0x3f9eb851eb851eb8ull)){
            if(!append(events,0x47,127,true)||!append(events,14,127,true))return false;
        }else if(!append(events,0x49,127,true))return false;
    }else if(!state.contact_event_active&&!state.cabinet.mode&&!state.cabinet.enabled){
        state.cabinet.mode=2;state.cabinet.impulse=5;state.contact_event_active=1;
    }
    state.quiet_updates=0;return true;
}
}

bool update_main_motion(const MainMotionInput& input,MainMotionState& output,
                        MainBalanceEffects& output_events) noexcept {
    for(float v:{input.selected_scalar,input.speed_metric,input.angle_change,input.gravity,
                 input.orientation_shape,input.middle_response_gain,input.middle_motion_gain,
                 input.contact_damping,input.selection_damping})if(!bounded(v))return false;
    for(const auto group:{words(input.force),words(input.moment),words(input.forward),words(input.reference_direction)})
        for(float v:group)if(!bounded(v))return false;
    for(float v:input.divisors){const auto magnitude=bits(v)&0x7fffffffu;if(magnitude<0x35800000u||magnitude>0x49800000u)return false;}
    for(const auto* group:{&output.vehicle.position,&output.vehicle.motion,&output.vehicle.orientation,&output.vehicle.angular_motion})
        for(float v:*group)if(!bounded(v))return false;
    MainMotionState state;std::memcpy(&state,&output,sizeof state);
    MainBalanceEffects events{};
    auto& car=state.vehicle;
    const auto moment=words(input.moment),force=words(input.force);
    for(unsigned axis=0;axis<3;++axis)
        car.angular_motion[axis]=signed_clamp(math::add32(math::div32(moment[axis],input.divisors[axis+1]),car.angular_motion[axis]));
    state.diagnostic_marker={2,2,25};
    if(state.active_contacts!=15&&negative(car.angular_motion[2])==negative(car.orientation[2]))
        car.orientation[2]=shaped(car.orientation[2],input.orientation_shape,0x3ff0000000000000ull);
    state.diagnostic_marker={2,2,26};
    if(state.active_contacts!=15)
        car.orientation[0]=shaped(car.orientation[0],input.orientation_shape,0x3ff6666666666666ull);
    car.angular_motion[1]=math::sub32(car.angular_motion[1],
        math::add32(math::mul32(input.middle_response_gain,input.angle_change),
                    math::mul32(input.middle_motion_gain,car.angular_motion[1])));

    if(((input.frame+4u)&7u)==0){
        std::int32_t level=0;
        if(state.active_contacts){
            if(!math::truncate(math::multiply(from_single(input.speed_metric),from_bits(0x405f400000000000ull)),level))return false;
            if(level>127)level=127;
        }
        const std::array<std::uint32_t,32>* commands;
        if(state.contact_types&0xccu){
            if(!state.sound_active){if(!append(events,0xb7,0,true))return false;state.sound_active=1;}
            if(!append(events,0xb8,std::bit_cast<std::uint32_t>(level),true))return false;
            // The original signed shift/divide has a well-defined bounded
            // active-race domain; reject rather than overflow signed C++ math.
            if(level < -0x10000000||level>0x0fffffff)return false;
            level=std::int32_t(std::int64_t(level)*8/10);commands=&input.rough_commands;
        }else{
            if(state.sound_active&&!append(events,0xb8,0,true))return false;
            level/=2;commands=&input.contact_commands;
        }
        if(level){
            const auto choice=random_next(state.random_state);
            if(std::int32_t(choice&255u)<level&&!append(events,(*commands)[choice&31u],0,false))return false;
        }
    }
    if(positive(input.selected_scalar)&&!greater(input.speed_metric,0x3fa7b425ed097b42ull)){
        int order=0;
        if(!math::compare(from_single(input.speed_metric),from_bits(0x3fa7b425ed097b42ull),order))return false;
        if(order<0){
            CrossVector3 crossed;
            if(!rounded_cross_product({input.forward.x,input.forward.y,input.forward.z},
                                      {input.reference_direction.x,input.reference_direction.y,input.reference_direction.z},crossed))return false;
            auto correction=math::multiply(from_single(crossed.y),from_bits(0x400921fb54442d18ull));
            correction=math::multiply(correction,from_bits(0x3f8eb851eb851eb8ull));
            car.angular_motion[1]=narrow(math::subtract(from_single(car.angular_motion[1]),correction));
        }
    }
    for(unsigned axis=0;axis<3;++axis)car.motion[axis]=math::add32(car.motion[axis],math::div32(force[axis],input.divisors[0]));
    car.motion[1]=math::sub32(car.motion[1],input.gravity);
    for(unsigned axis=0;axis<3;++axis)car.orientation[axis]=math::add32(car.orientation[axis],car.angular_motion[axis]);
    if(!input.mode){car.motion[0]=0;car.motion[2]=0;}
    for(unsigned axis=0;axis<3;++axis)car.position[axis]=math::add32(car.position[axis],car.motion[axis]);
    if(!contact_events(input,state,events))return false;

    if(positive(input.selected_scalar)){
        math::Number damping;
        if(input.descriptor==3)
            damping=math::subtract(from_bits(0x3fefae147ae147aeull),
                math::multiply(from_single(input.selected_scalar),from_bits(0x3ffccccccccccccdull)));
        else{
            damping=math::subtract(from_bits(0x3fefe76c8b439581ull),
                from_single(math::mul32(input.selected_scalar,input.selection_damping)));
            int order=0;if(!math::compare(damping,{},order))return false;if(order<0)damping={};
        }
        const auto factor=narrow(damping);
        for(auto& v:car.motion)v=math::mul32(factor,v);
    }else if(state.active_contacts){
        float damping=input.contact_damping;
        if(input.scene_correction_active&&!correct_scene_contact_damping(input.scene_geometry,damping,damping))return false;
        car.motion[0]=math::mul32(damping,car.motion[0]);
        car.motion[2]=math::mul32(damping,car.motion[2]);
    }
    if(input.mode==2)for(unsigned axis:{0u,2u})
        car.motion[axis]=narrow(math::multiply(from_single(car.motion[axis]),from_bits(0x3fef95810624dd2full)));
    float metric;
    if(!recovered_scalar_metric({0,0,car.motion[1]},{0,car.motion[0],car.motion[2]},metric))return false;
    if(greater(metric,0x3ff284bda12f684cull)){
        const auto factor=math::divide(from_bits(0x3ff284bda12f684cull),from_single(metric));
        for(auto& v:car.motion)v=narrow(math::multiply(from_single(v),factor));
    }
    for(auto& angle:car.orientation)if(!fold_angle_once(angle))return false;
    for(const auto* group:{&car.position,&car.motion,&car.orientation,&car.angular_motion})
        for(float v:*group)if(!math::finite(v))return false;
    std::memcpy(&output,&state,sizeof state);output_events=events;return true;
}
} // namespace ps2::native
