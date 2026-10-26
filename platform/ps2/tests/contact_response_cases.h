#pragma once
#include "contact_response.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace contact_response_cases {
using namespace ps2::native;
struct Case { ContactResponseInput input; float previous; };
using Words=std::array<std::uint32_t,5>;
inline float value(std::uint32_t word){return std::bit_cast<float>(word);}
inline std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
inline Words words(const ContactResponseOutput& out){return {bits(out.response.x),bits(out.response.y),bits(out.response.z),bits(out.selected_scalar),std::uint32_t(out.active)};}
inline std::uint32_t next(std::uint32_t& s){s^=s<<13;s^=s>>17;s^=s<<5;return s;}
inline ContactResponseOutput sentinel(){return {{value(0x7f800001u),value(0xff800000u),value(0x80000000u)},value(0xffc12345u),true};}
inline void hash(std::uint64_t& h,const Words& words,bool accepted){h=(h^std::uint32_t(accepted))*1099511628211ull;for(auto w:words)h=(h^w)*1099511628211ull;}
template<class Visit>bool each(Visit visit,bool extended){
    std::uint32_t seed=0xa5180c7du;
    const auto arbitrary=[&](){return value(next(seed));};
    const auto small=[&](int range,int scale){return float(int(next(seed)%unsigned(2*range+1))-range)/float(scale);};
    for(unsigned i=0;i<(extended?18000u:768u);++i){
        Case c{};c.input.contact.status=i%4?0x80u:0x0fu;
        c.input.offset={small(2048,256),small(2048,256),small(2048,256)};
        c.input.contact.direction={small(1024,1024),small(1024,1024),small(1024,1024)};
        c.input.contact.gap=small(1024,32);c.previous=small(1024,32);
        c.input.parameter50=value(((117+next(seed)%26u)<<23)|(next(seed)&0x7fffffu));
        c.input.parameter54=value(((117+next(seed)%26u)<<23)|(next(seed)&0x7fffffu));
        if(!c.input.contact.status||!(c.input.contact.status&128)){
            c.input.offset={arbitrary(),arbitrary(),arbitrary()};c.input.contact.direction={arbitrary(),arbitrary(),arbitrary()};
            c.input.contact.gap=arbitrary();c.input.parameter50=arbitrary();c.input.parameter54=arbitrary();c.previous=arbitrary();
        }
        if(!visit(c))return false;
    }
    constexpr std::array<std::uint32_t,12> values{0,0x80000000u,1,0x80000001u,0x007fffffu,0x807fffffu,0x00800000u,0x80800000u,0x3f800000u,0xbf800000u,0x41800000u,0xc1800000u};
    for(auto a:values)for(auto b:values)for(auto parameter:{0x3a800000u,0x447a0000u,0x47800000u}){
        Case c{};c.input.contact.status=128;c.input.offset={value(a),0,value(b)};
        c.input.contact.direction={1,value(b),value(a)};c.input.contact.gap=value(a);
        c.input.parameter50=value(parameter);c.input.parameter54=1146;c.previous=value(b);
        if(!visit(c))return false;
    }
    return true;
}
#ifndef _EE
inline float add(float a,float b){volatile float r=a+b;return r;}
inline float sub(float a,float b){volatile float r=a-b;return r;}
inline float mul(float a,float b){volatile float r=a*b;return r;}
inline float div(float a,float b){volatile float r=a/b;return r;}
inline float root(float a){volatile float r=float(std::sqrt(double(a)));return r;}
inline std::array<float,3> cross(const std::array<float,3>& a,const std::array<float,3>& b){
    return {sub(mul(a[1],b[2]),mul(a[2],b[1])),sub(mul(a[2],b[0]),mul(a[0],b[2])),sub(mul(a[0],b[1]),mul(a[1],b[0]))};
}
inline bool mathematical(const Case& c,ContactResponseOutput& out){
    if(!(c.input.contact.status&128)){out={{0,0,0},c.previous,false};return true;}
    const auto& d=c.input.contact.direction;const std::array<float,3> v{c.input.offset.x,c.input.offset.y,c.input.offset.z};
    auto n=cross(cross(d,v),d);
    const auto length=root(add(add(mul(n[0],n[0]),mul(n[1],n[1])),mul(n[2],n[2])));
    if(double(length)<=std::bit_cast<double>(std::uint64_t(0x380fffffff9fdba8ull)))n={0,0,1};
    else for(auto& x:n)x=div(x,length);
    const auto dot=add(add(mul(n[0],v[0]),mul(n[1],v[1])),mul(n[2],v[2]));
    if(dot<0)return false;
    const auto magnitude=root(dot);
    const auto denominator=add(c.input.parameter54,mul(mul(magnitude,c.input.parameter50),magnitude));
    const auto numerator=mul(c.input.parameter50,c.input.parameter54);
    volatile double half=double(numerator)*0.5,quotient=half/double(denominator);
    const auto gain=mul(float(quotient),c.input.contact.gap);
    out={{mul(gain,d[0]),mul(gain,d[1]),mul(gain,d[2])},c.input.contact.gap>=c.previous?c.input.contact.gap:c.previous,true};return true;
}
#endif
} // namespace contact_response_cases
