#pragma once
#include "main_balance_geometry.h"
#include <bit>
#include <cstdint>
namespace main_geometry_cases {
using namespace ps2::native;
struct Case {MainBalanceGeometryInput input;MainBalanceGeometryState state;};
using Input=std::array<std::uint32_t,16>;
using Output=std::array<std::uint32_t,7>;
inline float value(std::uint32_t v){return std::bit_cast<float>(v);}
inline std::uint32_t bits(float v){return std::bit_cast<std::uint32_t>(v);}
inline Input words(const Case&c){return {bits(c.input.longitudinal[0]),bits(c.input.longitudinal[1]),bits(c.input.lateral[0]),bits(c.input.lateral[1]),bits(c.input.balance[0]),bits(c.input.balance[1]),bits(c.input.steering),bits(c.input.middle_orientation),c.input.selected_index,bits(c.state.first.x),bits(c.state.first.y),bits(c.state.first.z),bits(c.state.second.x),bits(c.state.second.y),bits(c.state.second.z),c.state.published_index};}
inline Case input(const Input&w){return {{{value(w[0]),value(w[1])},{value(w[2]),value(w[3])},{value(w[4]),value(w[5])},value(w[6]),value(w[7]),w[8]},{{value(w[9]),value(w[10]),value(w[11])},{value(w[12]),value(w[13]),value(w[14])},w[15]}};}
inline Output words(const MainBalanceGeometryState&s){return {bits(s.first.x),bits(s.first.y),bits(s.first.z),bits(s.second.x),bits(s.second.y),bits(s.second.z),s.published_index};}
inline std::uint32_t next(std::uint32_t&s){s^=s<<13;s^=s>>17;s^=s<<5;return s;}
template<std::size_t N>void hash(std::uint64_t&h,const std::array<std::uint32_t,N>&w){for(auto v:w)h=(h^v)*1099511628211ull;}
inline Case base(){return {{{1,2},{0.25f,-0.5f},{0.125f,-0.25f},0.0625f,-0.125f,3},{{1,2,3},{-1,-2,-3},0xa55a5678}};}
template<class Visit>bool each(Visit visit,bool extended){
    constexpr std::array<std::uint32_t,14> edge{0,0x80000000u,1,0x80000001u,0x7fffffu,0x807fffffu,0x800000u,0x80800000u,0x3f800000u,0xbf800000u,0x40490fdbu,0xc0490fdbu,0x41000000u,0xc1000000u};
    for(unsigned i=0;i<16;++i)if(i!=8&&i!=15)for(auto v:edge){auto w=words(base());w[i]=v;if(!visit(input(w)))return false;}
    for(unsigned i=0;i<16;++i)if(i!=8&&i!=15)for(unsigned sign=0;sign<2;++sign){auto w=words(base());const auto limit=i<4?0x47800000u:i<6?0x41800000u:i<8?0x41000000u:0x49800000u;w[i]=limit|(sign?0x80000000u:0);if(!visit(input(w)))return false;}
    for(unsigned signs=0;signs<256;++signs){Input w{};for(unsigned i=0;i<8;++i)w[i]=(signs&(1u<<i))?0x80000000u:0;w[8]=0xffffffffu;w[15]=0xabcdef01;if(!visit(input(w)))return false;}
    std::uint32_t random=0x897c4213u;
    for(unsigned i=0;i<(extended?20000u:1024u);++i){Input w{};for(unsigned j=0;j<16;++j){auto raw=next(random);if(j==8||j==15){w[j]=raw;continue;}const auto bound=j<4?0x47800000u:j<6?0x41800000u:j<8?0x41000000u:0x49800000u;w[j]=(raw&0x80000000u)|(raw%(bound+1));}if(!visit(input(w)))return false;}
    return true;
}
}
