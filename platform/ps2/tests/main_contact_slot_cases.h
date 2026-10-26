#pragma once
#include "main_contact_slot.h"
#include <array>
#include <bit>
#include <cstdint>
namespace main_slot_cases {
using namespace ps2::native;
struct Case {MainContactSlotInput input;MainContactSlotState state;};
using Words=std::array<std::uint32_t,26>;
inline float value(std::uint32_t v){return std::bit_cast<float>(v);}
inline std::uint32_t bits(float v){return std::bit_cast<std::uint32_t>(v);}
inline std::uint32_t next(std::uint32_t&s){s^=s<<13;s^=s>>17;s^=s<<5;return s;}
inline Words words(const MainContactSlotState&s,const MainContactSlotOutput&o){Words w{};w[0]=s.packed_types;w[1]=s.type_mask;unsigned i=2;for(const auto&a:{s.history,s.published_middle,s.gain,s.metric,s.lateral})for(float v:a)w[i++]=bits(v);w[22]=bits(o.response.x);w[23]=bits(o.response.y);w[24]=bits(o.response.z);w[25]=o.active;return w;}
inline void hash(std::uint64_t&h,const Words&w){for(auto v:w)h=(h^v)*1099511628211ull;}
inline std::array<ContactNormalPair,8192> table(){std::array<ContactNormalPair,8192> t;for(unsigned i=0;i<t.size();++i)t[i]={float(1+i%8)/8.0f,1.0f+float(i%13)/4.0f};return t;}
inline Case base(unsigned slot){Case c;c.input.slot=std::uint8_t(slot);c.input.contact={{0.25f,1,-0.5f},0.125f,3};c.input.vector={1,0,0.5f};c.input.axis_direction={0.5f,0,1};c.input.reference_middle=2;c.input.factor=0.25f;c.input.object_parameter=1000;c.input.parameters={value(0x7f800001),1,2,0.01f,0.02f};c.input.control=1;c.state.packed_types=0xabcd;c.state.type_mask=0x93450000;c.state.history={0.0625f,0.125f,0.25f,0.5f};for(unsigned i=0;i<4;++i){c.state.published_middle[i]=value(0x7fc00001+i);c.state.gain[i]=value(0xffc00001+i);c.state.metric[i]=value(0x80000000u+i);c.state.lateral[i]=value(0x7f800001+i);}return c;}
template<class Visit>bool each(Visit visit,bool extended){
    for(unsigned slot=0;slot<4;++slot)for(unsigned status=0;status<256;++status){auto c=base(slot);c.input.contact.status=status|0x9a3c0000u;if(!visit(c))return false;}
    constexpr std::array<std::uint32_t,15> gaps{0,0x80000000u,1,0x80000001u,0x007fffffu,0x00800000u,0x3e4ccccbu,0x3e4cccccu,0x3e4ccccdu,0x3e4cccceu,0xbe4cccccu,0xbe4ccccdu,0x41800000u,0xc1800000u,0x3f800000u};
    for(unsigned slot=0;slot<4;++slot)for(auto gap:gaps)for(unsigned mode=0;mode<4;++mode){auto c=base(slot);c.input.contact.gap=value(gap);c.input.contact.status=mode&1?0x80:0;c.state.history[slot^1]=mode&2?0.125f:0;if(!visit(c))return false;}
    // Every finite binary32 exponent in the bounded scalar domain, including
    // subnormal and signed-zero paths. Tiny vectors keep normalization unused.
    for(unsigned exponent=0;exponent<=130;++exponent)for(unsigned mode=0;mode<4;++mode){auto c=base(mode);c.input.contact.status=128;c.input.contact.gap=value((exponent<<23)|(mode&1?0x7fffff:0));c.input.factor=value((mode&2?0x80000000u:0)|1);if(!visit(c))return false;}
    // A nonzero middle coordinate can take the metric gate with zero XZ.
    // The original table expression still runs; there is no divide-by-zero.
    for(unsigned slot=0;slot<4;++slot)for(unsigned signs=0;signs<4;++signs)for(unsigned middle=0;middle<2;++middle){auto c=base(slot);c.input.vector={value(signs&1?0x80000000u:0),middle?-1.0f:1.0f,value(signs&2?0x80000000u:0)};if(!visit(c))return false;}
    for(unsigned slot=0;slot<4;++slot)for(unsigned signs=0;signs<4;++signs)for(auto small:{1u,0x1a000000u}){auto c=base(slot);c.input.vector={value(small|(signs&1?0x80000000u:0)),1,value(signs&2?0x80000000u:0)};if(!visit(c))return false;}
    std::uint32_t random=0xc370112du;
    for(unsigned i=0;i<(extended?24000u:1024u);++i){auto c=base(i%4);c.input.contact.status=(next(random)&255u)|0x39120000u;
        c.input.contact.gap=float(int(next(random)%4097)-2048)/128.0f;
        for(auto&h:c.state.history)h=float(int(next(random)%4097)-2048)/128.0f;
        c.input.control=float(int(next(random)%129)-64)/16.0f;c.input.factor=float(int(next(random)%129)-64)/16.0f;
        c.input.object_parameter=float(next(random)%65537u);
        for(unsigned j=1;j<5;++j)c.input.parameters[j]=float(int(next(random)%131073u)-65536);
        for(auto&d:c.input.contact.direction)d=float(int(next(random)%65)-32)/4.0f;
        c.input.vector={float(int(next(random)%257)-128)/16.0f,0,float(1+next(random)%256)/16.0f};
        c.input.axis_direction={float(int(next(random)%65)-32)/16.0f,value(0x7f800001),float(int(next(random)%65)-32)/16.0f};
        if(i%7==0)c.input.vector={};if(i%11==0)c.input.contact.gap=value(0x3e4cccccu+(i&1));
        c.state.packed_types=std::uint16_t(next(random));c.state.type_mask=next(random);
        if(!visit(c))return false;
    }
    return true;
}
}
