#pragma once
#include "contact_result.h"
#include <bit>
#include <cstdint>
#ifndef _EE
#include <cmath>
#endif
namespace contact_result_cases {
using namespace ps2::native;
using Words=std::array<std::uint32_t,5>;
inline Words words(const ContactResult& r){return {std::bit_cast<std::uint32_t>(r.direction[0]),std::bit_cast<std::uint32_t>(r.direction[1]),std::bit_cast<std::uint32_t>(r.direction[2]),std::bit_cast<std::uint32_t>(r.gap),r.status};}
inline float value(std::uint32_t word){return std::bit_cast<float>(word);}
inline std::uint32_t next(std::uint32_t& r){r^=r<<13;r^=r>>17;r^=r<<5;return r;}
struct Case { ContactPolygon polygon;ContactVertex point;ContactResult previous; };
inline std::array<ContactNormalPair,8192> table(){
    std::array<ContactNormalPair,8192> result;
    for(unsigned i=0;i<result.size();++i)result[i]={value(0x3e000000u+i*511u),value(0x3fc00000u+i*257u)};
    return result;
}
template<class Visit>bool each(Visit visit,bool extended){
    std::uint32_t r=0x4f7b1823u;
    const auto arbitrary=[&](){return value(next(r));};
    const auto bounded=[&](){const auto b=next(r);return value((b&0x80000000u)|(b%0x49800001u));};
    const auto divisor=[&](){const auto b=next(r);return value((b&0x80000000u)|(0x35800000u+b%(0x49800000u-0x35800000u+1)));};
    for(unsigned i=0;i<(extended?40000u:2048u);++i){
        Case c{};
        c.previous={{arbitrary(),arbitrary(),arbitrary()},arbitrary(),next(r)};
        c.point={bounded(),bounded(),bounded()};
        for(auto& v:c.polygon.vertices)v={bounded(),arbitrary(),bounded()};
        if(i%3==0){
            c.polygon.attributes=0;c.point={arbitrary(),arbitrary(),arbitrary()};
            for(auto& coefficient:c.polygon.coefficients)coefficient=arbitrary();
        }else if(i%3==1){
            c.polygon.attributes=(next(r)&~0x00800000u)|1u;
            c.polygon.coefficients={bounded(),divisor(),bounded(),bounded()};
        }else{
            c.polygon.attributes=next(r)|0x00800000u;c.point.y=arbitrary();
            c.polygon.coefficients={arbitrary(),arbitrary(),arbitrary(),divisor()};
        }
        if(!visit(c))return false;
    }
    constexpr std::array<std::uint32_t,14> boundaries{0,0x80000000u,1,0x80000001u,0x7fffffu,0x807fffffu,
        0x800000u,0x80800000u,0x35800000u,0xb5800000u,0x3f800000u,0xbf800000u,0x49800000u,0xc9800000u};
    for(auto a:boundaries)for(auto b:boundaries)for(unsigned branch=0;branch<2;++branch){
        Case c{};c.point={value(a),value(b),value(a)};
        c.polygon.vertices[0]={value(b),0,value(a)};c.polygon.vertices[1]={value(a),0,value(b)};
        c.polygon.attributes=branch?0x008f0000u:0x000f0001u;
        c.polygon.coefficients={value(a),value(branch?b:0x35800000u),value(b),value(branch?0x35800000u:a)};
        if(!visit(c))return false;
    }
    return true;
}
#ifndef _EE
inline float add(float a,float b){volatile float v=a+b;return v;}
inline float sub(float a,float b){volatile float v=a-b;return v;}
inline float mul(float a,float b){volatile float v=a*b;return v;}
inline float div(float a,float b){volatile float v=a/b;return v;}
inline ContactResult mathematical(const Case& c,std::span<const ContactNormalPair> table){
    auto result=c.previous;
    const auto& p=c.polygon;const auto& q=c.point;const auto& k=p.coefficients;
    if(!p.attributes){result.status=15;return result;}
    result.status=(p.attributes>>16)&0x8f;
    if(result.status&0x80){
        const auto a=mul(sub(p.vertices[0].x,q.x),sub(p.vertices[1].z,q.z));
        const auto b=mul(sub(p.vertices[0].z,q.z),sub(p.vertices[1].x,q.x));
        result.gap=div(value(std::bit_cast<std::uint32_t>(sub(a,b))&0x7fffffffu),k[3]);
        for(unsigned i=0;i<3;++i)result.direction[i]=k[i];
    }else{
        result.gap=sub(div(sub(sub(mul(value(std::bit_cast<std::uint32_t>(k[0])^0x80000000u),q.x),mul(k[2],q.z)),k[3]),k[1]),q.y);
        const auto length=add(add(mul(k[0],k[0]),mul(k[1],k[1])),mul(k[2],k[2]));
        const auto word=std::bit_cast<std::uint32_t>(length);
        const auto row=((word>>11)&8191u)^4096u;
        const int power=63-int((word>>24)&127u);
        const auto slope=float(std::ldexp(double(table[row].slope),power));
        const auto intercept=float(std::ldexp(double(table[row].intercept),power));
        const auto mantissa=value((word&0x7fffffu)|0x3f800000u);
        const auto factor=sub(intercept,mul(mantissa,slope));
        for(unsigned i=0;i<3;++i)result.direction[i]=mul(k[i],factor);
    }
    return result;
}
#endif
inline void hash(std::uint64_t& state,const Words& input){for(auto w:input)state=(state^w)*1099511628211ull;}
} // namespace contact_result_cases
