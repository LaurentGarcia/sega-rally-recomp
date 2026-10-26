#pragma once
#include "contact_point_preparation.h"
#include <array>
#include <bit>
#include <cstdint>
namespace contact_point_cases {
using Case=std::array<std::uint32_t,18>;
using Result=std::array<std::uint32_t,36>;
inline ps2::native::AffinePoint3 point(const Case&c,unsigned n){return {std::bit_cast<float>(c[n]),std::bit_cast<float>(c[n+1]),std::bit_cast<float>(c[n+2])};}
inline std::array<ps2::native::AffinePoint3,4> local(const Case&c){return {point(c,6),point(c,9),point(c,12),point(c,15)};}
inline Result words(const ps2::native::ContactPointPreparation&r){
    Result out{};unsigned n=0;
    const auto append=[&](const auto&p){for(float v:{p.x,p.y,p.z})out[n++]=std::bit_cast<std::uint32_t>(v);};
    for(const auto&p:r.matrix.columns)append(p);append(r.matrix.translation);
    for(const auto&p:r.offsets)append(p);for(const auto&p:r.points)append(p);return out;
}
inline Case base(){return {0,0,0,0x41200000,0xc1a00000,0x41f00000,
    0xc0000000,0xbf800000,0x3f000000,0x40000000,0xbf800000,0x3f000000,
    0x40000000,0x3f800000,0xbf000000,0xc0000000,0x3f800000,0xbf000000};}
inline std::uint32_t next(std::uint32_t&s){s^=s<<13;s^=s>>17;s^=s<<5;return s;}
template<class Test>bool each(Test test,bool extended){
    for(unsigned mask=0;mask<64;++mask){Case c{};for(unsigned i=0;i<18;++i)c[i]=((mask>>(i%6))&1u)<<31;if(!test(c))return false;}
    constexpr std::array<std::uint32_t,23> edge{0,1,0x007fffff,0x00800000,0x32800000,0x33800000,
        0x3f490fda,0x3f490fdb,0x3f490fdc,0x3fc90fda,0x3fc90fdb,0x3fc90fdc,
        0x40490fda,0x40490fdb,0x40490fdc,0x4096cbe3,0x4096cbe4,0x4096cbe5,
        0x40c90fda,0x40c90fdb,0x40c90fdc,0x40ffffff,0x41000000};
    for(auto angle:edge)for(unsigned sign=0;sign<2;++sign)for(unsigned axis=0;axis<3;++axis){auto c=base();c[axis]=angle|(sign<<31);if(!test(c))return false;}
    for(unsigned exponent=0;exponent<=129;++exponent)for(auto fraction:{0u,1u,0x3fffffu,0x400000u,0x7ffffeu,0x7fffffu}){
        auto c=base();c[0]=(exponent<<23)|fraction;c[1]=c[0]^0x80000000u;c[2]=c[0];if(!test(c))return false;
    }
    for(unsigned lane=3;lane<18;++lane)for(auto value:{0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,0x49800000u,0xc9800000u}){
        auto c=base();c[0]=0x3eaaaaab;c[1]=0xbf555555;c[2]=0x3f800000;c[lane]=value;if(!test(c))return false;
    }
    const unsigned steps=extended?8192u:128u;
    for(unsigned i=0;i<=steps;++i){auto c=base();c[0]=0x3f000000u+i*(0x01000000u/steps);c[1]=c[0]^0x80000000u;c[2]=c[0];if(!test(c))return false;}
    std::uint32_t state=0x7a4ef01bu;
    for(unsigned i=0;i<(extended?40000u:2048u);++i){Case c;for(unsigned j=0;j<18;++j){auto r=next(state);c[j]=(r&0x80000000u)|((r&0x7fffffffu)%(j<3?0x41000001u:0x49800001u));}if(i%3==0)for(unsigned j=0;j<3;++j)c[j]=std::bit_cast<std::uint32_t>(float(int(next(state)%16001)-8000)/1024.0f);if(!test(c))return false;}
    return true;
}
inline void hash(std::uint64_t&h,const Case&c,const Result&r){for(auto v:c)h=(h^v)*1099511628211ull;for(auto v:r)h=(h^v)*1099511628211ull;}
}
