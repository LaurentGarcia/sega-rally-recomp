#pragma once
#include "contact_pass_response.h"
#include <bit>
#include <cstdint>
namespace contact_pass_response_cases {
using namespace ps2::native;
struct Case {ContactPassResponseInput input;ContactPassResponseState state;};
inline float value(std::uint32_t v){return std::bit_cast<float>(v);}
inline std::uint32_t bits(float v){return std::bit_cast<std::uint32_t>(v);}
inline std::uint32_t next(std::uint32_t& s){s^=s<<13;s^=s>>17;s^=s<<5;return s;}
using Words=std::array<std::uint32_t,13>;
inline Words words(const ContactPassResponseState& s){
    Words w{};for(unsigned i=0;i<3;++i){w[i]=bits(s.car.motion[i]);w[3+i]=bits(s.car.candidate_position[i]);w[6+i]=bits(s.car.candidate_orientation[i]);}
    w[9]=std::bit_cast<std::uint32_t>(s.car.feedback[0]);w[10]=std::bit_cast<std::uint32_t>(s.car.feedback[1]);
    w[11]=s.contact_mask;w[12]=bits(s.selected_scalar);return w;
}
inline void hash(std::uint64_t& h,const Words&w){for(auto v:w)h=(h^v)*1099511628211ull;}
template<class Visit>bool each(Visit visit,bool extended){
    std::uint32_t random=0x718e2d49u;
    for(unsigned i=0;i<(extended?4224u:640u);++i){
        Case c{};const unsigned mask=i%16,mode=(i/16)%8;
        c.input.response_parameter=1000;c.input.gain_parameter=1146;c.input.scalar_divisor=1146;
        c.input.feedback_enabled=(i%3)==0;
        c.input.bases={float(int(next(random)%257)-128),float(int(next(random)%257)-128),0.25f};
        c.state.car.motion={0.125f,-0.25f,0.0625f};
        c.state.car.candidate_position={11,value(0x7f800001u),22};
        c.state.car.candidate_orientation={value(0xffc00001u),0.5f,value(0x80000000u)};
        c.state.car.feedback={-123,456};c.state.contact_mask=0xdeadbeefu;c.state.selected_scalar=value(0x7fc00000u);
        for(unsigned j=0;j<4;++j){
            auto& contact=c.input.contacts[j];contact.status=(mask&(1u<<j)?0x80u:0u)|(next(random)&15);
            c.input.offsets[j]={float(int(next(random)%33)-16)*0.125f,1.0f,float(1+next(random)%16)*0.125f};
            contact.direction=(j&1)?std::array<float,3>{0,0,1}:std::array<float,3>{1,0,0};
            const auto magnitude=float(1+next(random)%32)*0.0009765625f;
            contact.gap=mode==0?0.0f:mode==1?value(0x80000000u):mode==2?-magnitude:magnitude;
            if(!(contact.status&0x80)){
                contact.direction={value(0x7f800001u),value(0xffffffffu),value(0xff800000u)};
                contact.gap=value(0x7fc00000u);c.input.offsets[j].y=value(0x7f800001u);
            }
        }
        if(!visit(c))return false;
    }
    return true;
}
} // namespace contact_pass_response_cases
