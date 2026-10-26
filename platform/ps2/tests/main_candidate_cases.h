#pragma once
#include "main_candidate.h"
#include "recovered_scalar_metric.h"
#include <bit>
#include <cstdint>
#ifndef _EE
#include <cmath>
#endif
namespace main_candidate_cases {
using ps2::native::MainCandidateState;
using Words=std::array<std::uint32_t,22>;
inline float value(std::uint32_t b){return std::bit_cast<float>(b);}
inline std::uint32_t bits(float v){return std::bit_cast<std::uint32_t>(v);}
inline std::uint32_t next(std::uint32_t& s){s^=s<<13;s^=s>>17;s^=s<<5;return s;}
inline Words words(const MainCandidateState& s){
    Words w{};unsigned n=0;
    for(const auto* group:{&s.position,&s.pending_position_delta,&s.motion,&s.orientation,&s.angular_motion})
        for(float v:*group)w[n++]=bits(v);
    w[n++]=s.response_marker;
    for(const auto* group:{&s.candidate_position,&s.candidate_orientation})for(float v:*group)w[n++]=bits(v);
    return w;
}
inline MainCandidateState state(const Words& w){
    MainCandidateState s;unsigned n=0;
    for(auto* group:{&s.position,&s.pending_position_delta,&s.motion,&s.orientation,&s.angular_motion})
        for(float& v:*group)v=value(w[n++]);
    s.response_marker=w[n++];
    for(auto* group:{&s.candidate_position,&s.candidate_orientation})for(float& v:*group)v=value(w[n++]);
    return s;
}
inline void hash(std::uint64_t& h,const Words& w){for(auto v:w)h=(h^v)*1099511628211ull;}
template<class Visit>bool each(Visit visit,bool extended){
    std::uint32_t random=0x6819b37du;
    for(unsigned i=0;i<(extended?16384u:1024u);++i){
        Words w{};
        for(unsigned j=0;j<15;++j){const auto b=next(random);w[j]=(b&0x80000000u)|(b%0x49800001u);}
        w[15]=next(random)|0x80000000u;
        for(unsigned j=16;j<22;++j)w[j]=next(random);
        if(i%4==0)w[3]=w[4]=w[5]=0;
        if(!visit(state(w)))return false;
    }
    constexpr std::array<std::uint32_t,18> boundary{0,0x80000000u,1,0x80000001u,0x7fffffu,0x807fffffu,
        0x800000u,0x80800000u,0x3b03126eu,0x3b03126fu,0x3dccccccu,0x3dcccccdu,
        0xbdccccccu,0xbdcccccdu,0x3f800000u,0xbf800000u,0x49800000u,0xc9800000u};
    for(unsigned axis=0;axis<15;++axis)for(auto b:boundary){
        MainCandidateState s;s.position={1,-2,3};s.motion={0.125f,-0.25f,0.5f};
        s.orientation={0.25f,-0.5f,0.75f};s.angular_motion={-0.125f,0.25f,-0.5f};
        s.pending_position_delta={0.125f,0.0625f,-0.25f};s.response_marker=0xabcdef01u;
        auto w=words(s);w[axis]=b;if(!visit(state(w)))return false;
    }
    for(unsigned axis=0;axis<3;++axis)for(unsigned sign=0;sign<2;++sign)for(unsigned step=0;step<65;++step){
        MainCandidateState s;s.response_marker=0xffffffffu;
        s.pending_position_delta[axis]=value(0x3b03124fu+step+(sign?0x80000000u:0));
        if(!visit(s))return false;
    }
    return true;
}
#ifndef _EE
inline float add(float a,float b){volatile float r=a+b;return r;}
inline float mul(float a,float b){volatile float r=a*b;return r;}
inline bool mathematical(MainCandidateState& s){
    auto r=s;auto& d=r.pending_position_delta;
    if(std::fabs(double(d[1]))>0.1)d[1]=std::signbit(d[1])?-0.1f:0.1f;
    float metric;
    if(!ps2::native::recovered_scalar_metric({0,0,d[1]},{0,d[0],d[2]},metric))return false;
    if(double(metric)>0.002){
        r.response_marker=15;
        for(unsigned axis:{0u,2u}){
            r.motion[axis]=add(r.motion[axis],mul(d[axis],0.6f));
            volatile double scaled=double(d[axis])*std::bit_cast<double>(std::uint64_t(0x3fd9999980000000ull));
            d[axis]=float(scaled);
        }
        for(unsigned axis=0;axis<3;++axis){r.position[axis]=add(r.position[axis],d[axis]);d[axis]=0;}
    }
    for(unsigned axis=0;axis<3;++axis){r.candidate_position[axis]=add(r.position[axis],r.motion[axis]);r.candidate_orientation[axis]=add(r.orientation[axis],r.angular_motion[axis]);}
    s=r;return true;
}
#endif
} // namespace main_candidate_cases
