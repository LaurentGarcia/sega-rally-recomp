#include "main_balance_update.h"
#include "main_balance_math.h"
#include "cabinet_steering.h"
#include "paired_balance.h"
#include "vehicle_arithmetic.h"
#include <algorithm>
#include <bit>
#include <limits>

namespace ps2::native {
namespace {
namespace vm=vehicle_math;
using N=vm::Number;
float f(std::uint32_t bits) noexcept {return std::bit_cast<float>(bits);}
float abs_value(float x) noexcept {return f(std::bit_cast<std::uint32_t>(x)&0x7fffffffu);}
float neg(float x) noexcept {return f(std::bit_cast<std::uint32_t>(x)^0x80000000u);}
N d(float x) noexcept {return vm::from_single(x);}
N c(std::uint64_t bits) noexcept {return vm::from_bits(bits);}
N integer(std::int32_t x) noexcept {return vm::from_integer(x);}
constexpr std::uint64_t pi=0x400921fb54442d18ull,half_pi=0x3ff921fb54442d18ull;
constexpr std::uint64_t tenth=0x3fb999999999999aull,fifth=0x3fc999999999999aull;
constexpr std::uint64_t three_tenths=0x3fd3333333333333ull;
constexpr std::uint64_t four_tenths=0x3fd999999999999aull;
constexpr std::uint64_t one_tenth=0x3ff199999999999aull;
std::int32_t plus(std::int32_t a,std::int32_t b) noexcept {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a)+std::bit_cast<std::uint32_t>(b));
}
struct Arithmetic {
    bool valid=true;
    float check(float x) noexcept {valid&=vm::finite(x);return x;}
    float q(N x) noexcept {return check(vm::narrow(x));}
    float add(float a,float b) noexcept {return check(vm::add32(a,b));}
    float sub(float a,float b) noexcept {return check(vm::sub32(a,b));}
    float mul(float a,float b) noexcept {return check(vm::mul32(a,b));}
    float div(float a,float b) noexcept {return check(vm::div32(a,b));}
    float sqrt(float a) noexcept {return check(vm::sqrt32(a));}
    int compare(N a,N b) noexcept {int result=0;valid&=vm::compare(a,b,result);return result;}
    std::int32_t trunc(N a) noexcept {std::int32_t result=0;valid&=vm::truncate(a,result);return result;}
    float sine(float a) noexcept {float result=0;valid&=main_balance_sine(a,result);return result;}
};
bool emit(MainBalanceEffects& effects,std::uint32_t code,std::uint32_t value=0,bool parameter=false) noexcept {
    if(effects.count==effects.commands.size())return false;
    effects.commands[effects.count++]={code,value,parameter};return true;
}
bool scalar_domain(float x,float limit=1048576) noexcept {return vm::finite(x)&&abs_value(x)<=limit;}
bool point_domain(const AffinePoint3& p) noexcept {return scalar_domain(p.x)&&scalar_domain(p.y)&&scalar_domain(p.z);}
template<std::size_t Size>bool scalars(const std::array<float,Size>& values) noexcept {
    for(float x:values)if(!scalar_domain(x))return false;return true;
}
float bounded_rate(float x) noexcept {return x<0?0:x>=8800?8800:x;}

bool engine_commands(const MainBalanceInputs& input,float accelerator,MainBalancePower& power,
                     float rate,MainBalanceEffects& effects,Arithmetic& m) noexcept {
    if(!power.pitch_limit||power.pitch_limit>127||power.level_limit>127||power.last_level>127)return false;
    const float product=m.mul(m.q(integer(static_cast<std::int32_t>(power.pitch_limit))),rate);
    const auto pitch=static_cast<std::uint32_t>(std::clamp<std::int32_t>(m.trunc(vm::divide(d(product),integer(8800))),1,static_cast<std::int32_t>(power.pitch_limit)));
    auto level=static_cast<std::uint32_t>(m.trunc(d(m.mul(m.q(integer(static_cast<int>(power.level_limit))),accelerator))));
    if(level<power.last_level)level=power.last_level-std::max<std::uint32_t>((power.last_level-level)>>2,1u);
    if(power.upshift_hold)level=power.level_limit;else if(power.downshift_hold)level=0;
    const bool auxiliary=(input.contact_mask&0x300u)!=0;
    if(power.auxiliary_active&&!auxiliary){
        if(!emit(effects,power.auxiliary_level_command,0,true))return false;
        power.auxiliary_active=0;
    }else if(auxiliary&&!power.auxiliary_active){
        if(!emit(effects,power.auxiliary_start_command))return false;
        power.last_pitch=0;power.auxiliary_active=1;
    }
    if(pitch!=power.last_pitch||level!=power.last_level){
        if(power.auxiliary_active&&!emit(effects,power.auxiliary_level_command,pitch,true))return false;
        if(!emit(effects,power.level_command,level,true)||!emit(effects,power.pitch_command,pitch,true))return false;
        power.last_pitch=pitch;power.last_level=level;
    }
    return m.valid;
}
bool occasional_commands(const MainBalanceAssets& a,float drive,MainBalancePower& p,
                         MainBalanceEffects& effects,Arithmetic& m) noexcept {
    if(p.occasional_mode==0){
        if(m.compare(d(drive),c(0x3fc3333333333333ull))>0){
            const auto old=p.occasional_count;p.occasional_count=plus(old,1);
            if(old>60)p.occasional_mode=1;
        }else p.occasional_count=p.occasional_count<0?0:plus(p.occasional_count,-1);
    }else if(p.occasional_mode==1){
        if(m.compare(d(drive),c(0x3f847ae147ae147bull))<0){
            if(!emit(effects,0x6c,64,true))return false;
            p.occasional_count=0;p.occasional_mode=2;
        }
    }else if(p.occasional_mode==2){
        const std::uint64_t product=std::uint64_t(p.random_state)*0x5d588b65u;
        p.random_state=(std::uint32_t(product)+2u*std::uint32_t(product>>32)+std::uint32_t((product>>31)&1))&0x7fffffffu;
        if(!emit(effects,a.occasional_commands[p.random_state&7]))return false;
        p.occasional_mode=0;
    }
    return m.valid;
}
void select_gear(const MainBalanceInputs& in,MainBalanceVehicle& v,MainBalancePower& p,
                 float accelerator,Arithmetic& m) noexcept {
    if(in.manual_selection){
        if(in.requested_gear==v.gear){p.selection_delay=0;return;}
        const auto old=p.selection_delay;p.selection_delay=plus(old,1);
        if(old<=6)return;
        if(in.requested_gear){
            if(in.requested_gear<v.gear)p.upshift_hold=6;
            else p.downshift_hold=in.shift_variant==1?-6:-8;
        }
        v.gear=in.requested_gear;return;
    }
    if(in.operating_mode==0){v.gear=1;return;}
    const auto old=p.selection_delay;p.selection_delay=plus(old,1);
    if(old<=29)return;
    int selected=static_cast<int>(v.gear);
    if(m.compare(d(v.rate),c(0x40bef00000000001ull))>0&&m.compare(d(p.previous_slip),c(tenth))<0)++selected;
    else if(m.compare(d(v.rate),vm::add(vm::multiply(d(accelerator),integer(2750)),c(0x40a9c80000000001ull)))<0)--selected;
    selected=std::clamp(selected,1,4);
    if(static_cast<unsigned>(selected)!=v.gear)p.selection_delay=0;
    v.gear=static_cast<unsigned>(selected);
}
bool update_power(const MainBalanceAssets& a,const MainBalanceInputs& in,
                  MainBalanceUpdateState& state,MainBalanceEffects& effects,Arithmetic& m) noexcept {
    auto& v=state.vehicle;auto& p=state.power;
    auto accelerator=m.div(m.q(integer(std::clamp(int(in.accelerator_sample)-48,0,143))),143);
    if(p.upshift_hold>0){p.upshift_hold=plus(p.upshift_hold,-1);accelerator=f(0x3e99999a);}
    if(p.downshift_hold<0)p.downshift_hold=plus(p.downshift_hold,1);
    if(in.operating_mode>1)accelerator=0;
    select_gear(in,v,p,accelerator,m);
    const unsigned gear=(in.operating_mode==0||p.upshift_hold)?0:v.gear;
    if(gear>=a.gear_ratios.size())return false;
    if(!(in.frame&1)&&!engine_commands(in,accelerator,p,v.rate,effects,m))return false;
    p.ratio=m.mul(a.gear_ratios[gear],a.ratio_scale);
    AffinePoint3 direction;
    if(!main_balance_rotate(m.add(v.middle_orientation,v.steering),{0,0,1},direction))return false;
    const float projected=m.add(m.add(m.mul(v.motion.x,direction.x),m.mul(v.motion.y,direction.y)),m.mul(v.motion.z,direction.z));
    const float inertia=m.add(m.div(a.inertia_ratio_scale,p.ratio),a.inertia_base);
    const auto rolling=projected<0?0:m.q(vm::multiply(vm::divide(d(m.mul(m.mul(in.speed_metric,60),60)),c(0x3ffe28c731eb6950ull)),d(p.ratio)));
    if(gear){
        p.target_rate=bounded_rate(m.add(rolling,p.correction));
        auto delta=m.sub(p.target_rate,v.rate);
        if(abs_value(delta)>200)delta=delta<0?-200:200;
        v.rate=m.add(v.rate,delta);
        const float effective=in.variant_mode?m.q(vm::multiply(d(v.rate),vm::add(integer(1),d(m.mul(p.slip,a.slip_boost))))):v.rate;
        p.slip=std::min(abs_value(m.div(m.sub(effective,rolling),effective)),1.0f);
    }else {v.rate=p.target_rate;p.slip=0;}
    v.phase_step=m.q(vm::divide(d(in.speed_metric),c(three_tenths)));
    v.phase=m.add(v.phase,v.phase_step);
    if(v.rate<=1000||projected<=0)p.slip=0;
    p.factor=v.rate<6000&&m.compare(d(accelerator),c(0x3fe999999999999aull))>0
        ?m.q(vm::subtract(integer(1),vm::divide(vm::subtract(integer(6000),d(v.rate)),integer(6000)))):1;
    p.previous_slip=p.slip;
    const auto index=m.trunc(vm::divide(vm::divide(d(v.rate),integer(100)),c(one_tenth)));
    const auto whole_rate=m.trunc(d(v.rate));
    if(index<0||std::size_t(index)+1>=a.power_curve.size())return false;
    const auto fraction=m.q(vm::divide(integer(whole_rate%100),integer(100)));
    const float curve=m.add(a.power_curve[unsigned(index)],m.mul(m.sub(a.power_curve[unsigned(index)+1],a.power_curve[unsigned(index)]),fraction));
    const float drive=m.mul(curve,m.mul(m.mul(accelerator,a.drive_scale),in.reference_scale));
    if(!occasional_commands(a,drive,p,effects,m))return false;
    p.target_rate=bounded_rate(m.add(v.rate,m.div(m.sub(drive,a.friction),inertia)));
    float force=m.mul(m.sub(p.target_rate,rolling),inertia);
    force=gear&&in.operating_mode<=1?m.q(vm::divide(d(m.mul(force,p.ratio)),c(three_tenths))):0;
    const int brake_half=(int(in.brake_sample)-48)/2;
    auto brake=m.q(vm::divide(integer(brake_half),integer(64)));
    if(m.compare(d(brake),c(0x3fe3333333333333ull))>0)v.flags|=16;else v.flags&=~16u;
    if(in.operating_mode>1)brake=f(0x3f000000);
    brake=m.mul(m.mul(a.brake_scale,in.reference_scale),brake);
    if(brake<0)brake=0;
    p.force=m.q(projected<0?vm::add(d(force),d(brake)):vm::subtract(d(force),d(brake)));
    return m.valid;
}

bool low_speed_cabinet(const MainBalanceAssets& a,const MainBalanceInputs& in,
                       MainBalanceCabinet& s,Arithmetic& m) noexcept {
    const float scale=m.q(vm::add(vm::add(d(s.mix),c(fifth)),vm::add(d(s.mix),c(fifth))));
    s.previous_steering=s.current_steering;
    const float steering=cabinet_steering(in.steering_sample);
    const float average=m.q(vm::multiply(d(m.add(m.add(m.add(s.response_history[0],s.response_history[1]),s.response_history[2]),s.response_history[3])),c(0x3fd0000000000000ull)));
    const float delta_average=m.q(vm::multiply(d(m.add(m.add(m.add(s.delta_history[0],s.delta_history[1]),s.delta_history[2]),s.delta_history[3])),c(0x3fd0000000000000ull)));
    const int index=std::min<std::int32_t>(m.trunc(d(m.mul(abs_value(steering),63))),63);
    if(index<0)return false;
    N center=vm::divide(d(a.cabinet_center[unsigned(index)]),integer(63));
    if(steering<0)center=vm::negate(center);
    float response=m.q(vm::multiply(d(m.mul(neg(s.center_gain),scale)),center));
    const float delta=m.sub(steering,s.previous_steering);
    response=m.sub(m.add(response,m.mul(m.mul(neg(delta),s.delta_gain),scale)),m.mul(m.sub(average,m.mul(delta_average,s.history_gain)),scale));
    const float filtered=m.q(vm::multiply(d(m.add(response,s.filtered)),c(0x3fe0000000000000ull)));
    s.response_history[in.frame&3]=response;s.delta_history[in.frame&3]=delta;
    s.current_steering=steering;s.filtered=filtered;
    auto quantized=m.trunc(vm::multiply(vm::divide(d(filtered),integer(20)),d(m.q(integer(s.output_limit)))));
    auto previous=s.previous_quantized;
    if(!previous)previous=quantized;
    if(previous<quantized){previous=quantized;quantized=plus(quantized,1);}
    else if(previous>quantized){previous=quantized;quantized=plus(quantized,-1);}
    s.previous_quantized=previous;
    if(s.output_limit<0||s.output_limit>127)return false;
    quantized=std::clamp(quantized,-s.output_limit,s.output_limit);
    if(!quantized){
        if(in.cabinet_kind==1){s.output=0;return m.valid;}
        quantized=(in.frame&3)?-1:1;
    }
    s.output=static_cast<std::uint8_t>(quantized>0?quantized+127:-quantized-65);
    if(in.cabinet_kind==1&&in.cabinet_screen!=4)s.output=0;
    return m.valid;
}
float cabinet_curve(const MainBalanceAssets& a,float value,float mix,Arithmetic& m) noexcept {
    const float position=std::min(abs_value(value),63.0f);
    const auto index=m.trunc(d(position));
    if(index<0||index>63){m.valid=false;return 0;}
    float low=a.cabinet_low[unsigned(index)],high=a.cabinet_high[unsigned(index)];
    if(index!=63){
        const auto fraction=m.sub(position,m.q(integer(index)));
        low=m.add(low,m.mul(m.sub(a.cabinet_low[unsigned(index)+1],low),fraction));
        high=m.add(high,m.mul(m.sub(a.cabinet_high[unsigned(index)+1],high),fraction));
    }
    auto result=m.add(low,m.mul(m.sub(high,low),mix));
    if(value<0)result=neg(result);
    return result;
}
bool moving_cabinet(const MainBalanceAssets& a,const MainBalanceInputs& in,
                    float steering,float force,float lateral,MainBalanceCabinet& s,Arithmetic& m) noexcept {
    const float delta=m.sub(steering,s.prior_input);s.prior_input=steering;
    if(s.mode==0){
        const auto magnitude=d(abs_value(lateral));
        const auto shaped=m.q(vm::add(vm::multiply(d(m.sqrt(abs_value(lateral))),c(0x3feccccccccccccdull)),vm::multiply(vm::multiply(magnitude,c(tenth)),magnitude)));
        const auto wave=m.sine(m.q(vm::multiply(d(shaped),c(pi))));
        auto response=m.mul(m.mul(force,a.cabinet_force_gain),wave);
        if(m.compare(d(abs_value(steering)),c(0x3fa999999999999aull))>0)response=m.sub(response,m.mul(delta,a.cabinet_delta_gain));
        response=m.add(response,m.mul(steering,s.steering_gain));
        const float mapped=cabinet_curve(a,response,s.mix,m);
        auto amount=m.trunc(d(mapped));
        if(amount==std::numeric_limits<std::int32_t>::min())return false;
        amount=std::min<std::int32_t>(amount<0?-amount:amount,31);
        float sign=mapped;
        if(!amount){
            if(in.cabinet_kind==1){s.output=0;return m.valid;}
            if(in.frame&3){sign=1;amount=1;}else{sign=-1;amount=-1;}
        }
        s.output=static_cast<std::uint8_t>(amount+(sign<0?127:191));
    }else if(s.mode==1){
        auto amount=m.trunc(vm::multiply(d(m.q(integer(s.impulse))),vm::add(d(s.mix),c(tenth))));
        amount=std::clamp<std::int32_t>(amount,-31,31);
        s.output=static_cast<std::uint8_t>(amount<0?-64-amount:amount-128);
        s.countdown=plus(s.countdown,-1);if(s.countdown==-1)s.mode=0;
    }else if(s.mode==2){s.enabled=1;s.mode=0;s.output=static_cast<std::uint8_t>(std::uint32_t(s.impulse)+16);}
    else if(s.mode==3){s.mode=0;s.enabled=0;s.output=16;}
    else s.mode=0;
    return m.valid;
}
float unloaded_feedback(bool loaded,MainBalanceSound& sound,MainBalanceEffects& effects,bool& valid) noexcept {
    if(!loaded){
        valid&=emit(effects,0x96);sound.unloaded_count=plus(sound.unloaded_count,1);
        return sound.unloaded_count>59?0:5;
    }
    float result=0;
    if(sound.unloaded_count>10){valid&=emit(effects,0x47,127,true);result=5;}
    sound.unloaded_count=0;return result;
}
bool surface_sound(const MainBalanceAssets& a,const MainBalanceInputs& in,
                   float slip,unsigned selected,MainBalanceSound& s,
                   MainBalanceEffects& effects,Arithmetic& m) noexcept {
    if(slip<0){s.active=0;s.selected=15;return true;}
    if(in.frame&7)return true;
    if(s.selected!=selected){
        s.active=0;s.last_value=0;s.last_target=0;s.selected=selected;
        const auto& row=a.sound_parameters[selected];
        s.initial_command=row[0];s.continuation_command=row[1];s.initial_bias=row[2];s.continuation_bias=row[3];
        if(!emit(effects,0xb5,0,true))return false;
    }
    std::uint32_t target=0;
    if(slip==0)s.active=0;
    else{
        const auto base=m.trunc(vm::multiply(vm::multiply(d(slip),integer(100)),c(four_tenths)));
        const auto bias=s.active?s.continuation_bias:s.initial_bias;
        if(bias>127||base<0)return false;
        target=std::min<std::uint32_t>(std::uint32_t(base)+bias,127u);
        if(!s.active){
            if(!emit(effects,s.initial_command,0,true))return false;
            if(s.initial_command==0xb3&&!emit(effects,0x95,127,true))return false;
            s.last_value=0xffffffffu;s.last_target=target;s.active=1;
        }else if((target>s.last_target?target-s.last_target:s.last_target-target)>3){
            s.last_target=target;
            if(!emit(effects,0xb5,0,true)||!emit(effects,s.continuation_command,0,true))return false;
            if(s.continuation_command==0xb3&&!emit(effects,0x95,127,true))return false;
        }
    }
    if(s.last_value!=target){if(!emit(effects,0xb5,target,true))return false;s.last_value=target;}
    return m.valid;
}
float limit_force(float ratio,float mix,Arithmetic& m) noexcept {
    return m.q(vm::add(vm::multiply(vm::multiply(vm::subtract(integer(1),d(mix)),d(ratio)),d(ratio)),d(m.mul(m.sqrt(ratio),mix))));
}
float bounded_angle(N angle,Arithmetic& m) noexcept {
    if(m.compare(angle,integer(0))<0)angle=vm::negate(angle);
    return m.compare(angle,c(half_pi))>0?f(0x3fc90fdb):m.q(angle);
}
bool update(const MainBalanceAssets& a,const MainBalanceInputs& in,
            MainBalanceUpdateState& s,MainBalanceEffects& effects,Arithmetic& m) noexcept {
    const auto selection=select_vehicle_parameters(in.contact_mask,a.parameters);
    s.selected_parameters=selection.record;
    const auto parameter=[&](unsigned i){return f(selection.record.words[i]);};
    for(unsigned i:{0u,1u,2u,9u,10u})if(!scalar_domain(parameter(i)))return false;
    if(!update_power(a,in,s,effects,m))return false;
    PairedBalance pair;if(!make_paired_balance(s.vehicle.contact_weights,pair))return false;
    auto first=pair.sums[0],second=pair.sums[1];
    float longitudinal_first=0,longitudinal_second=0,bounce;
    const bool loaded=m.mul(first,second)!=0;
    s.unloaded=loaded?0:1;
    bounce=unloaded_feedback(loaded,s.sound,effects,m.valid);
    if(loaded){
        const auto distribution=in.variant_mode?f(0x3e4ccccd):parameter(2);
        longitudinal_first=m.mul(distribution,s.power.force);
        longitudinal_second=m.sub(s.power.force,longitudinal_first);
        bounce=m.mul(neg(s.contact_sums[0]),bounce);
    }
    const float gain=m.q(vm::add(integer(1),vm::multiply(d(in.speed_metric),c(three_tenths))));
    const auto total=m.add(first,second);
    if(total==0)s.balance_history=0;
    else{
        const auto ratio=m.div(m.sub(first,second),total);
        const auto difference=m.sub(ratio,s.balance_history);
        const auto filtered=m.sub(ratio,m.mul(difference,parameter(difference>0?9:10)));
        const float wave=m.sine(m.q(vm::multiply(d(filtered),c(pi))));
        const float shaped=m.add(filtered,m.mul(wave,a.balance_shape[filtered>0?0:1]));
        first=m.q(vm::multiply(d(parameter(0)),vm::add(integer(1),d(shaped))));
        second=m.q(vm::multiply(d(parameter(1)),vm::subtract(integer(1),d(shaped))));
        s.balance_history=shaped;
    }
    if(abs_value(s.contact_sums[0])<a.lateral_deadzone)s.contact_sums[0]=0;
    else s.contact_sums[0]=m.q(vm::subtract(d(s.contact_sums[0]),d(s.contact_sums[0]<0?neg(a.lateral_deadzone):a.lateral_deadzone)));
    const auto capacity=[&](float load){return m.q(vm::multiply(vm::multiply(vm::multiply(d(in.reference_scale),integer(1000)),d(load)),d(gain)));};
    auto front_capacity=capacity(first);
    const auto front_angle=bounded_angle(vm::divide(vm::multiply(d(s.contact_sums[0]),c(0x401921fb54442d18ull)),integer(3)),m);
    const float wave=m.sine(front_angle);
    const float moment=m.add(in.incoming_moment,m.mul(m.mul(bounce,wave),front_capacity));
    float front=m.q(vm::multiply(d(s.contact_sums[0]<0?wave:neg(wave)),d(front_capacity)));
    const auto length=[&](float lateral,float longitudinal){return m.q(vm::multiply(d(m.sqrt(m.add(m.mul(lateral,lateral),m.mul(longitudinal,longitudinal)))),vm::add(integer(1),d(s.power.slip))));};
    auto combined=length(front,longitudinal_first);
    if(combined>front_capacity){
        const auto scale=limit_force(m.div(front_capacity,combined),a.limit_mix[0],m);
        longitudinal_first=m.mul(longitudinal_first,scale);
        front=m.q(vm::multiply(vm::multiply(d(scale),c(one_tenth)),d(front)));
    }
    const auto taper=[&](float value,unsigned offset){const auto product=m.mul(abs_value(s.contact_sums[offset]),s.contact_sums[offset+1]);return product<f(0x3ca3d70a)?m.mul(value,m.div(product,f(0x3ca3d70a))):value;};
    front=taper(front,0);
    if(m.compare(d(in.speed_metric),c(0x3f97b425ed097b42ull))<0){if(!low_speed_cabinet(a,in,s.cabinet,m))return false;}
    else if(!moving_cabinet(a,in,s.vehicle.steering,m.add(moment,front),s.contact_sums[0],s.cabinet,m))return false;
    if(in.operating_mode==2)s.cabinet.output=0;
    if(longitudinal_first<0)pair.balances[0]=0;
    const auto rear_capacity=capacity(second);
    const auto rear_angle=bounded_angle(vm::multiply(d(s.contact_sums[2]),c(pi)),m);
    float rear=m.mul(rear_capacity,m.sine(rear_angle));
    combined=length(rear,longitudinal_second);
    if(combined>rear_capacity){
        const auto scale=limit_force(m.div(rear_capacity,combined),a.limit_mix[1],m);
        longitudinal_second=m.mul(longitudinal_second,scale);
        rear=m.q(vm::multiply(vm::multiply(d(scale),c(one_tenth)),d(rear)));
    }
    if(s.contact_sums[2]>=0)rear=neg(rear);
    rear=taper(rear,2);
    if(longitudinal_second<0)pair.balances[1]=0;
    const auto inertia=m.add(m.div(a.inertia_ratio_scale,s.power.ratio),a.inertia_base);
    const auto force_difference=m.sub(s.power.force,m.mul(m.add(longitudinal_first,longitudinal_second),s.power.factor));
    s.power.correction=m.q(vm::divide(vm::divide(vm::multiply(vm::multiply(d(a.feedback_rate_scale),c(three_tenths)),d(force_difference)),d(s.power.ratio)),d(inertia)));
    if(m.mul(longitudinal_first,longitudinal_second)==0)s.power.slip=0;
    const bool extra=m.compare(d(abs_value(in.angle_history)),c(fifth))>0&&m.compare(d(s.contact_sums[3]),c(tenth))>0;
    if(in.variant_mode){
        if(m.compare(d(s.power.slip),c(0x3fc3333333333333ull))<0&&extra)s.power.slip=f(0x3e19999a);
        if(m.compare(d(s.power.slip),c(0x3fc3333333333333ull))<0)s.power.slip=0;
    }else if(s.power.slip==0&&extra)s.power.slip=f(0x3c23d70a);
    if(!surface_sound(a,in,s.power.slip,selection.index,s.sound,effects,m))return false;
    MainBalanceGeometryInput geometry;
    geometry.longitudinal={longitudinal_first,longitudinal_second};geometry.lateral={front,rear};
    geometry.balance=pair.balances;geometry.steering=s.vehicle.steering;
    geometry.middle_orientation=s.vehicle.middle_orientation;geometry.selected_index=selection.index;
    return m.valid&&publish_main_balance_geometry(geometry,s.accumulated);
}
bool domain(const MainBalanceAssets& a,const MainBalanceInputs& in,const MainBalanceUpdateState& s) noexcept {
    if(a.power_curve.size()<82||a.power_curve.size()>104||!scalars(a.limit_mix)||!scalars(a.balance_shape)||
       !scalars(a.cabinet_center)||!scalars(a.cabinet_low)||!scalars(a.cabinet_high)||!scalars(a.gear_ratios)||
       !scalars(s.vehicle.contact_weights)||!scalars(s.contact_sums)||!scalars(s.cabinet.response_history)||!scalars(s.cabinet.delta_history))return false;
    for(float x:a.power_curve)if(!scalar_domain(x))return false;
    for(float x:{a.lateral_deadzone,a.cabinet_delta_gain,a.cabinet_force_gain,a.ratio_scale,a.inertia_base,a.inertia_ratio_scale,
                 a.friction,a.drive_scale,a.brake_scale,a.feedback_rate_scale,a.slip_boost,
                 in.speed_metric,in.reference_scale,in.angle_history,in.incoming_moment,
                 s.vehicle.rate,s.vehicle.phase_step,s.vehicle.phase,s.balance_history,
                 s.cabinet.mix,s.cabinet.steering_gain,s.cabinet.filtered,s.cabinet.center_gain,s.cabinet.delta_gain,s.cabinet.history_gain,
                 s.cabinet.current_steering,s.cabinet.previous_steering,s.cabinet.prior_input,
                 s.power.correction,s.power.target_rate,s.power.ratio,s.power.previous_slip,s.power.factor,s.power.slip,s.power.force})if(!scalar_domain(x))return false;
    if(!point_domain(s.vehicle.motion)||!point_domain(s.accumulated.first)||!point_domain(s.accumulated.second)||
       !scalar_domain(s.vehicle.middle_orientation,8)||!scalar_domain(s.vehicle.steering,8)||
       s.vehicle.gear>4||in.requested_gear>4||s.vehicle.rate<0||s.vehicle.rate>8800||s.power.target_rate<0||s.power.target_rate>8800||
       in.speed_metric<0||in.reference_scale<=0||a.ratio_scale<=0||a.inertia_base<=0||a.inertia_ratio_scale<0||a.lateral_deadzone<0)return false;
    for(float ratio:a.gear_ratios)if(ratio<=0)return false;
    return true;
}
} // namespace
bool update_main_balance(const MainBalanceAssets& assets,const MainBalanceInputs& input,
                         MainBalanceUpdateState& state,MainBalanceEffects& effects) noexcept {
    if(!domain(assets,input,state))return false;
    auto next=state;MainBalanceEffects emitted;Arithmetic arithmetic;
    if(!update(assets,input,next,emitted,arithmetic))return false;
    state=next;effects=emitted;return true;
}
} // namespace ps2::native
