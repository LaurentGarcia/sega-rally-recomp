#pragma once
#include "contact_aggregate.h"
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>

namespace contact_aggregate_cases {
using namespace ps2::native;
struct Step { float accumulator,metric,sign; };
struct Case { ContactAggregateInput input; ContactAggregateState state; };
inline float value(std::uint32_t bits){return std::bit_cast<float>(bits);}
inline std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
inline std::uint32_t next(std::uint32_t& state){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
using Words=std::array<std::uint32_t,11>;
inline Words words(const ContactAggregateState& state){
    Words result{};
    for(unsigned i=0;i<3;++i){result[i]=bits(state.motion[i]);result[3+i]=bits(state.candidate_position[i]);result[6+i]=bits(state.candidate_orientation[i]);}
    result[9]=std::bit_cast<std::uint32_t>(state.feedback[0]);result[10]=std::bit_cast<std::uint32_t>(state.feedback[1]);return result;
}
inline void hash(std::uint64_t& state,std::uint32_t word){state=(state^word)*1099511628211ull;}
inline void hash(std::uint64_t& state,const Words& words){for(auto word:words)hash(state,word);}
template<class Visit> bool steps(Visit visit,bool extended){
    constexpr std::array<std::uint32_t,12> values{0,0x80000000u,1,0x80000001u,0x007fffffu,0x807fffffu,
        0x00800000u,0x35800000u,0x3f800000u,0xbf800000u,0x49800000u,0xc9800000u};
    for(auto a:values)for(auto m:values)for(auto s:values)
        if(!visit(Step{value(a),value(m&0x7fffffffu),value(s)}))return false;
    std::uint32_t seed=0x517ab291u;
    const auto random=[&](bool positive){const auto raw=next(seed);return value((positive?0u:raw&0x80000000u)|((next(seed)%147u)<<23)|(raw&0x7fffffu));};
    for(unsigned i=0;i<(extended?24000u:512u);++i)if(!visit(Step{random(false),random(true),random(false)}))return false;
    return true;
}
template<class Visit> bool each(Visit visit,bool extended){
    std::uint32_t seed=0xa51b7ce1u;
    const auto bounded=[&](){const auto raw=next(seed);return value((raw&0x80000000u)|((next(seed)%147u)<<23)|(raw&0x7fffffu));};
    const auto arbitrary=[&](){return value(next(seed));};
    const auto divisor=[&](){const auto raw=next(seed);return value((raw&0x80000000u)|((107u+next(seed)%40u)<<23)|(raw&0x7fffffu));};
    for(unsigned i=0;i<(extended?18000u:512u);++i){
        Case c{};c.input.signed_scalar=bounded();
        c.input.response_divisor=divisor();c.input.scalar_divisor=divisor();
        c.input.feedback_enabled=(i%3)==0;
        if(c.input.feedback_enabled){
            c.input.signed_scalar=value((next(seed)&0x807fffffu)|((next(seed)%136u)<<23));
            c.input.scalar_divisor=value((next(seed)&0x807fffffu)|((127u+next(seed)%10u)<<23));
        }
        for(auto& response:c.input.responses)response={bounded(),arbitrary(),bounded()};
        for(unsigned k=0;k<3;++k){c.input.bases[k]=bounded();c.state.motion[k]=bounded();c.state.candidate_position[k]=arbitrary();c.state.candidate_orientation[k]=arbitrary();}
        c.state.feedback={std::bit_cast<std::int32_t>(next(seed)),std::bit_cast<std::int32_t>(next(seed))};
        if(!visit(c))return false;
    }
    constexpr std::array<std::uint32_t,10> values{0,0x80000000u,1,0x80000001u,0x007fffffu,0x00800000u,
        0x3f800000u,0xbf800000u,0x49800000u,0xc9800000u};
    for(auto a:values)for(auto b:values)for(auto d:{0x35800000u,0xb5800000u,0x49800000u,0xc9800000u}){
        Case c{};c.input.signed_scalar=value(a);c.input.response_divisor=value(d);c.input.scalar_divisor=value(d);
        c.input.responses={{{value(a),value(0x7f800001u),value(b)},{value(b),0,value(a)},
                            {value(a^0x80000000u),0,value(a)},{value(b),0,value(b^0x80000000u)}}};
        c.input.bases={value(a),value(b),value(a)};c.state.motion={value(b),value(a),value(b)};
        c.state.candidate_position={value(0x7f800001u),value(0xffc00001u),value(0x80000000u)};
        c.state.candidate_orientation={value(0x7f800001u),value(0x80000000u),value(0xff800000u)};
        c.state.feedback={-123,456};if(!visit(c))return false;
    }
    return true;
}
#ifndef _EE
inline float add(float a,float b){volatile float value=a+b;return value;}
inline float div(float a,float b){volatile float value=a/b;return value;}
inline float step_math(const Step& step){
    const double metric=step.sign<0?-double(step.metric):double(step.metric);
    volatile double value=double(step.accumulator)-metric;return float(value);
}
inline ContactAggregateState mathematical(const Case& c){
    auto out=c.state;const auto& in=c.input;
    const auto scalar=div(in.signed_scalar,in.scalar_divisor);
    out.motion[2]=add(c.state.motion[2],scalar);
    const auto first=add(add(add(in.responses[0][0],in.responses[1][0]),in.responses[2][0]),in.responses[3][0]);
    const auto third=add(add(add(in.responses[0][2],in.responses[1][2]),in.responses[2][2]),in.responses[3][2]);
    out.motion[0]=add(div(first,in.response_divisor),c.state.motion[0]);
    out.motion[1]=add(div(third,in.response_divisor),c.state.motion[1]);
    out.candidate_position[0]=add(in.bases[0],out.motion[0]);out.candidate_position[2]=add(in.bases[1],out.motion[1]);
    out.candidate_orientation[1]=add(in.bases[2],out.motion[2]);
    if(in.feedback_enabled){
        volatile double first_feedback=double(scalar)*1000.0,second_feedback=double(scalar)*8000.0;
        const auto first_integer=std::int32_t(first_feedback);
        out.feedback={first_integer<0?-first_integer:first_integer,std::int32_t(second_feedback)};
    }
    return out;
}
#endif
} // namespace contact_aggregate_cases
