// Offline only: original firmware remains an ignored, owner-supplied input.
#include "car_render_bridge.h"
#include "runtime/tgp.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
using namespace ps2::native;
using Words=std::array<std::uint32_t,12>;
std::uint32_t bits(float x){return std::bit_cast<std::uint32_t>(x);}
float value(std::uint32_t x){return std::bit_cast<float>(x);}
Words words(const AffineTransform3&m){return {bits(m.columns[0].x),bits(m.columns[0].y),bits(m.columns[0].z),bits(m.columns[1].x),bits(m.columns[1].y),bits(m.columns[1].z),bits(m.columns[2].x),bits(m.columns[2].y),bits(m.columns[2].z),bits(m.translation.x),bits(m.translation.y),bits(m.translation.z)};}
struct Bus final:rt::TgpBus {
    std::vector<std::uint32_t> input,output;unsigned used=0;
    bool fifo_pop(std::uint32_t&v)override{if(used==input.size())return false;v=input[used++];return true;}
    void fifo_push(std::uint32_t v)override{output.push_back(v);}
    std::uint32_t mem_r(std::uint32_t)override{throw rt::TgpFatal("bridge external read");}
    void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("bridge external write");}
    void bank_w(std::uint32_t v)override{if(v)throw rt::TgpFatal("bridge bank change");}
};
void execute(rt::Tgp&t,Bus&bus){
    const auto before=t.count;rt::tgpgen::run(t,t.count+10000);
    if(t.count-before>=10000||t.pc!=0x77||bus.used!=bus.input.size()||!bus.output.empty())throw rt::TgpFatal("bridge command did not finish");
    if(t.tables||t.bank_reg||t.sincos_base||t.inv_base||t.isqrt_base)throw rt::TgpFatal("bridge table state changed");
    bus.input.clear();bus.used=0;
}
Words current(const rt::Tgp&t){Words r;for(unsigned i=0;i<12;++i)r[i]=t.data[0x200+t.data[7]+i];return r;}
void append(Bus&b,std::uint32_t op,AffinePoint3 v){b.input.insert(b.input.end(),{op,bits(v.x),bits(v.y),bits(v.z)});}
void rotate(Bus&b,unsigned axis,float angle){constexpr std::array<std::uint32_t,3> command{0x14802929,0x15002a2a,0x15802b2b};b.input.insert(b.input.end(),{command[axis],bits(angle)});}
std::uint64_t fingerprint=14695981039346656037ull,instructions=0;unsigned cases=0,live_cases=0;
void match(const Words&native,const Words&original,unsigned part){
    for(unsigned i=0;i<12;++i){
        if(native[i]!=original[i]){std::fprintf(stderr,"BRIDGE MISMATCH case=%u part=%u lane=%u native=%08x original=%08x\n",cases,part,i,native[i],original[i]);throw rt::TgpFatal("bridge matrix mismatch");}
        for(unsigned j=0;j<4;++j){fingerprint^=(native[i]>>(8*j))&255;fingerprint*=1099511628211ull;}
    }
}
void compare(const rt::Tgp&initial,const RenderPose&pose,const CarRenderGeometry&geometry,const AffineTransform3&view,const std::array<Words,6>* captured=nullptr){
    CarRenderBridge native;if(!build_car_render_bridge(pose,geometry,view,native))throw rt::TgpFatal("bridge native rejected case");
    if(captured){
        const std::array<Words,6> actual{words(native.shadow),words(native.body),words(native.wheels[0]),words(native.wheels[1]),words(native.wheels[2]),words(native.wheels[3])};
        for(unsigned i=0;i<6;++i)match(actual[i],(*captured)[i],i);
        ++live_cases;
    }
    rt::Tgp t=initial;Bus b;t.bus=&b;const auto start=t.count;const auto base=t.data[7],depth=t.data[9];
    const auto input=words(view);for(unsigned i=0;i<12;++i)t.data[0x200+base+i]=input[i];
    b.input.push_back(0x10002020);
    // Independent host arithmetic for the CPU's shadow arguments, with each
    // declared single/double boundary materialized before command submission.
    const auto h=[&](unsigned i){return value(pose.words.attributes[7+i]);};
    const auto add=[](float a,float z){volatile float r=a+z;return float(r);};
    const auto sub=[](float a,float z){volatile float r=a-z;return float(r);};
    volatile float sum=add(add(add(h(0),h(1)),h(2)),h(3));
    volatile double mean64=double(sum)*.25;volatile float mean=float(mean64);
    volatile double y64=double(mean)-std::bit_cast<double>(0x3f947ae147ae147bull);volatile float y=float(y64);
    volatile double p64=double(sub(add(h(0),h(1)),add(h(2),h(3))))*std::bit_cast<double>(0x3fce76c8b4395810ull);volatile float pitch=float(p64);
    volatile double r64=double(sub(add(h(0),h(2)),add(h(1),h(3))))*std::bit_cast<double>(0x3fd54fdf3b645a1dull);volatile float roll=float(r64);
    append(b,0x13802727,{value(pose.words.position[0]),y,value(pose.words.position[2])});
    rotate(b,1,value(pose.words.orientation[1]));rotate(b,0,pitch);rotate(b,2,roll);execute(t,b);match(words(native.shadow),current(t),0);
    b.input.insert(b.input.end(),{0x10802121,0x10002020});
    append(b,0x13802727,{value(pose.words.position[0]),value(pose.words.position[1]),value(pose.words.position[2])});
    rotate(b,1,value(pose.words.orientation[1]));rotate(b,0,value(pose.words.orientation[0]));rotate(b,2,value(pose.words.orientation[2]));execute(t,b);match(words(native.body),current(t),1);
    for(unsigned i=0;i<4;++i){
        b.input.push_back(0x10002020);auto offset=geometry.wheel_offsets[i];offset.y=add(offset.y,value(pose.words.attributes[3+i]));append(b,0x13802727,offset);
        if(i<2)rotate(b,1,value(pose.words.attributes[0]));
        rotate(b,0,value(pose.words.attributes[2]^0x80000000u));execute(t,b);match(words(native.wheels[i]),current(t),2+i);
        b.input.push_back(0x10802121);execute(t,b);
    }
    b.input.push_back(0x10802121);execute(t,b);
    if(t.data[7]!=base||t.data[9]!=depth||current(t)!=input)throw rt::TgpFatal("bridge saved matrix changed");
    instructions+=t.count-start;++cases;
}
}
int main(int argc,char**argv){try{
    if((argc!=2&&argc!=3)||std::fegetround()!=FE_TONEAREST)return 2;
    std::ifstream f(argv[1],std::ios::binary);const std::vector<unsigned char> firmware{std::istreambuf_iterator<char>(f),{}};
    if(firmware.size()!=6660)throw rt::TgpFatal("bridge firmware size differs");
    rt::Tgp initial;initial.reset();Bus startup;initial.bus=&startup;
    for(unsigned i=0;i<firmware.size()/4;++i)initial.prog[i]=std::uint32_t(firmware[4*i])|(std::uint32_t(firmware[4*i+1])<<8)|(std::uint32_t(firmware[4*i+2])<<16)|(std::uint32_t(firmware[4*i+3])<<24);
    startup.input={0x04000000};rt::tgpgen::run(initial,1000);
    if(initial.count!=108||initial.pc!=0x77||startup.output!=std::vector<std::uint32_t>{8})throw rt::TgpFatal("bridge startup differs");
    std::uint32_t rng=0x234981;
    const auto random=[&](float scale){rng=rng*1664525u+1013904223u;return float(std::int32_t(rng%65537)-32768)*scale;};
    for(unsigned n=0;n<2048;++n){
        RenderPose pose;for(auto&x:pose.words.position)x=bits(random(1.0f/64));for(auto&x:pose.words.orientation)x=bits(random(1.0f/8192));
        pose.words.attributes[0]=bits(random(1.0f/32768));pose.words.attributes[2]=bits(random(n<1024?1.0f/8192:32.0f));
        for(unsigned i=3;i<11;++i)pose.words.attributes[i]=bits(random(1.0f/32768));
        CarRenderGeometry geometry;for(auto&p:geometry.wheel_offsets)p={random(1.0f/8192),random(1.0f/32768),random(1.0f/8192)};
        AffineTransform3 view;for(auto&p:view.columns)p={random(1.0f/16384),random(1.0f/16384),random(1.0f/16384)};view.translation={random(1.0f/64),random(1.0f/64),random(1.0f/64)};
        compare(initial,pose,geometry,view);
    }
    if(argc==3){
        std::ifstream records_file(argv[2],std::ios::binary);const std::vector<unsigned char> records{std::istreambuf_iterator<char>(records_file),{}};
        const auto word=[&](unsigned at){return std::uint32_t(records.at(at))|(std::uint32_t(records.at(at+1))<<8)|(std::uint32_t(records.at(at+2))<<16)|(std::uint32_t(records.at(at+3))<<24);};
        if(records.size()<12||std::string(records.begin(),records.begin()+8)!="RLYBODY1")throw rt::TgpFatal("body record header");
        const auto count=word(8);if(!count||count>4096||records.size()!=12+472*count)throw rt::TgpFatal("body record extent");
        for(unsigned row=0;row<count;++row){unsigned at=12+472*row;const auto next=[&](){auto r=word(at);at+=4;return r;};RenderPose pose;
            for(auto*field:{&pose.words.position,&pose.words.motion,&pose.words.orientation})for(auto&v:*field)v=next();
            for(auto&v:pose.words.attributes)v=next();pose.flags=next();pose.progress=next();
            CarRenderGeometry geometry;for(auto&p:geometry.wheel_offsets)p={value(next()),value(next()),value(next())};
            AffineTransform3 view;for(auto&p:view.columns)p={value(next()),value(next()),value(next())};view.translation={value(next()),value(next()),value(next())};
            std::array<Words,6> captured;for(auto&m:captured)for(auto&v:m)v=next();compare(initial,pose,geometry,view,&captured);
        }
    }
    std::printf("CAR_RENDER_BRIDGE_ORACLE PASS cases=%u matrices=%u words=%u instructions=%llu hash=%016llx external_accesses=0 live_cases=%u\n",cases,cases*6,cases*72,(unsigned long long)instructions,(unsigned long long)fingerprint,live_cases);
}catch(const std::exception&e){std::fprintf(stderr,"bridge oracle: %s\n",e.what());return 1;}return 0;}
