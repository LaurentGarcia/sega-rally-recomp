// Invented full-scene inputs shared by SDL and native backend fixtures.
#pragma once
#include "runtime/video.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
namespace model2_fixture {
inline rt::GeoPoly polygon(std::vector<std::array<float,2>> points,unsigned renderer,bool checker=false,bool clipped=false){
    rt::GeoPoly p;p.num_vertices=uint8_t(points.size());p.z=100;p.luma=252;p.texheader[0]=uint16_t((renderer<<13)|(checker?0x8000:0));
    p.viewport[0]=clipped?17:0;p.viewport[1]=clipped?184:0;p.viewport[2]=clipped?199:495;p.viewport[3]=clipped?367:384;p.center[1]=384;
    for(size_t n=0;n<points.size();++n)p.v[n]={points[n][0],-points[n][1],{1.f,8.f,8.f}};return p;
}
struct Scene {
    std::vector<uint8_t> tiles=std::vector<uint8_t>(0x10000),chars=std::vector<uint8_t>(0x80000),palette=std::vector<uint8_t>(0x4000),translation=std::vector<uint8_t>(0xc000),luma=std::vector<uint8_t>(0x20000,255);
    std::vector<uint32_t> texture=std::vector<uint32_t>(0x80000);
    rt::VideoMem mem{palette.data(),translation.data(),luma.data(),texture.data(),texture.data(),0};
    rt::Video video{tiles.data(),chars.data()};
    Scene(){
        video.xhout_w(uint16_t(-84));video.xvout_w(uint16_t(-130));
        for(size_t i=0;i<translation.size();i+=2)translation[i]=uint8_t(((i/2)%0x2000)/32);
        palette[0x2000]=255;palette[0x2001]=127;palette[0x2002]=31;
        for(unsigned i=0;i<4096;++i){const uint16_t color=uint16_t(((i*7)&31)|(((i*11)&31)<<5)|(((i*17)&31)<<10));palette[2*i]=uint8_t(color);palette[2*i+1]=uint8_t(color>>8);}
        for(unsigned i=0;i<4096;++i)video.palette_w(i,palette.data(),translation.data());
    }
    void patterned_tiles(bool enabled){
        std::fill(chars.begin(),chars.end(),0);std::fill(tiles.begin(),tiles.end(),0);
        if(enabled){
            for(unsigned code=1;code<128;++code)for(unsigned y=0;y<8;++y)for(unsigned x=0;x<4;++x)chars[(code*32+y*4+x)^1]=uint8_t(((code+y+x)%15+1)<<4|((code+2*y+x)%15+1));
            for(unsigned layer=0;layer<4;++layer)for(unsigned i=0;i<4096;++i){const uint16_t value=uint16_t((1+(i+layer*17)%127)|(((i/11+layer)&1)?0x8000:0));tiles[(layer*4096+i)*2]=uint8_t(value);tiles[(layer*4096+i)*2+1]=uint8_t(value>>8);}
        }
        video.tile_memory_w();video.character_memory_w();
    }
    void cutout(bool enabled){
        std::fill(texture.begin(),texture.end(),enabled?0xffffffffu:0u);
        if(enabled)for(unsigned y=8;y<24;++y)for(unsigned x=8;x<24;++x){const unsigned offset=(y/2)*512+x/2,shift=(offset&1)*16+((y&1)?0:8)+((x&1)?0:4);texture[offset/2]&=~(15u<<shift);}
        ++mem.tex_generation;
    }
    std::vector<rt::GeoPoly> polys(unsigned variant){
        const std::vector<std::array<float,2>> square{{20.5f,200.5f},{84.5f,200.5f},{84.5f,264.5f},{20.5f,264.5f}};
        auto lower=polygon(square,0);lower.z=200;lower.texheader[3]=64;auto upper=polygon(square,3,variant&1);
        for(auto &v:upper.v){v.p[1]=(v.x-20.5f)*4;v.p[2]=(-v.y-200.5f)*4;}
        auto tri=polygon({{120.25f,20.5f},{240.75f,70.5f},{110.25f,160.5f}},2,variant&1,variant&2);
        auto polygon6=polygon({{300.5f,80.5f},{350.5f,100.5f},{365.5f,85.5f},{370.5f,140.5f},{330.5f,120.5f},{310.5f,140.5f}},0);polygon6.texheader[3]=64;
        return variant&4?std::vector<rt::GeoPoly>{lower,tri,upper,polygon6}:std::vector<rt::GeoPoly>{upper,tri,lower,polygon6};
    }
};
} // namespace model2_fixture
