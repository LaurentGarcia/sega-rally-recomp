// Original isolated CPU + geometry-firmware comparison. Private inputs only.
#include "scene_contact_damping.h"
#include "vehicle_arithmetic.h"
#include "refcore/i960_ref.h"
#include "runtime/tgp.h"
#include <bit>
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
namespace {
using namespace ps2::native;using Bytes=std::vector<std::uint8_t>;
Bytes read(const char* path){std::ifstream f(path,std::ios::binary);if(!f)throw rt::Fatal("missing private image");return {std::istreambuf_iterator<char>(f),{}};}
std::uint32_t word(const Bytes& b,std::size_t p){return b.at(p)|(std::uint32_t(b.at(p+1))<<8)|(std::uint32_t(b.at(p+2))<<16)|(std::uint32_t(b.at(p+3))<<24);}
std::uint32_t bits(float f){return std::bit_cast<std::uint32_t>(f);}float value(std::uint32_t x){return std::bit_cast<float>(x);}
struct Geometry:rt::TgpBus{
    std::vector<std::uint32_t> input,output;unsigned consumed=0;
    bool fifo_pop(std::uint32_t& v)override{if(consumed==input.size())return false;v=input[consumed++];return true;}
    void fifo_push(std::uint32_t v)override{if(output.size()>=2)throw rt::TgpFatal("scene excessive outputs");output.push_back(v);}
    std::uint32_t mem_r(std::uint32_t)override{throw rt::TgpFatal("scene external read");}
    void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("scene external write");}
    void bank_w(std::uint32_t v)override{if(v)throw rt::TgpFatal("scene bank switch");}
};
constexpr std::uint32_t base=0x210000,frame=base,published=base+0x1000,reference=base+0x1100;
struct Memory:rt::Bus{
    const Bytes& program;rt::Tgp geometry;Geometry io;unsigned output_index=0;std::uint64_t geometry_steps=0;
    std::array<std::uint32_t,0x4000/4> ram{};
    Memory(const Bytes& p,const rt::Tgp& initial):program(p),geometry(initial){geometry.bus=&io;for(unsigned i=0;i<ram.size();++i)ram[i]=0xa53b0000u^i;}
    std::uint32_t& at(std::uint32_t a){if(a<base||a>=base+ram.size()*4||a&3)throw rt::Fatal("scene RAM range");return ram[(a-base)/4];}
    void flush(){const auto before=geometry.count;rt::tgpgen::run(geometry,512);geometry_steps+=geometry.count-before;
        if(geometry.pc!=0x77||geometry.count-before>=512||io.consumed!=io.input.size()||geometry.tables||geometry.bank_reg)throw rt::Fatal("scene geometry completion");}
    std::uint32_t fetch(std::uint32_t a)override{if(a<0x5d5ee0||a>=0x5d6024||a&3)throw rt::Fatal("scene CPU range");return word(program,a-0x59f000);}
    std::uint32_t read_dword(std::uint32_t a)override{
        if(a==0x884000){flush();if(output_index>=io.output.size())throw rt::Fatal("scene empty result");return io.output[output_index++];}
        if(a!=0x213980&&!(a>=published&&a<published+24)&&!(a>=reference&&a<reference+12)&&!(a>=frame+0x160&&a<=frame+0x168))throw rt::Fatal("scene unexpected data read");return at(a);}
    void write_dword(std::uint32_t a,std::uint32_t v)override{if(a==0x884000){if(io.input.size()>=14)throw rt::Fatal("scene excessive command input");io.input.push_back(v);return;}
        if(a<frame+0x160||a>frame+0x168||a&3)throw rt::Fatal("scene unexpected data write");at(a)=v;}
    std::uint8_t read_byte(std::uint32_t)override{throw rt::Fatal("scene byte read");}
    std::uint16_t read_word(std::uint32_t)override{throw rt::Fatal("scene half read");}
    void write_byte(std::uint32_t,std::uint8_t)override{throw rt::Fatal("scene byte write");}
    void write_word(std::uint32_t,std::uint16_t)override{throw rt::Fatal("scene half write");}
    std::uint16_t flags(std::uint32_t)override{return rt::Cpu::BURST;}
};
unsigned total=0,accepted=0,corrected=0,rejected=0;std::uint64_t hash=14695981039346656037ull,cpu_steps=0,geometry_steps=0;
void compare(const Bytes& program,const rt::Tgp& initial,const SceneContactGeometry& input,float damping){
    float result=value(0x7fc12345);const bool ok=correct_scene_contact_damping(input,damping,result);++total;
    for(auto x:{bits(input.published_point.x),bits(input.published_point.y),bits(input.published_point.z),bits(input.reference_point.x),bits(input.reference_point.y),bits(input.reference_point.z),bits(input.direction.x),bits(input.direction.y),bits(input.direction.z),bits(damping),bits(result)}){hash^=x;hash*=1099511628211ull;}
    if(!ok){if(bits(result)!=0x7fc12345)throw rt::Fatal("scene rejection mutated result");++rejected;return;}
    Memory memory(program,initial);const auto write_point=[&](std::uint32_t a,AffinePoint3 p){memory.at(a)=bits(p.x);memory.at(a+4)=bits(p.y);memory.at(a+8)=bits(p.z);};
    write_point(published,input.published_point);write_point(published+12,input.direction);write_point(reference,input.reference_point);memory.at(0x213980)=reference;
    auto expected=memory.ram;
    const AffinePoint3 delta{vehicle_math::sub32(input.published_point.x,input.reference_point.x),vehicle_math::sub32(input.published_point.y,input.reference_point.y),vehicle_math::sub32(input.published_point.z,input.reference_point.z)};
    for(auto pair:{std::pair{frame+0x160,bits(delta.x)},std::pair{frame+0x164,bits(delta.y)},std::pair{frame+0x168,bits(delta.z)}})expected[(pair.first-base)/4]=pair.second;
    rt::I960Ref cpu(&memory);cpu.m_AC=0x3f001000;cpu.m_IP=0x5d5ee0;cpu.m_r[23]=published;cpu.m_r[29]=bits(damping);cpu.m_r[30]=0;cpu.m_r[31]=frame;
    unsigned steps=0;while(cpu.m_IP!=0x5d6024){if(++steps>100)throw rt::Fatal("scene CPU bound");cpu.execute_one();}memory.flush();
    if(cpu.m_r[29]!=bits(result)||memory.ram!=expected||memory.output_index!=memory.io.output.size()||
       (memory.io.input.size()!=7&&memory.io.input.size()!=14)||memory.io.input[0]!=0x2c805959u||
       (memory.io.input.size()==14&&memory.io.input[7]!=0x2c005858u)){
        std::fprintf(stderr,"scene mismatch case=%u original=%08x native=%08x\n",total,cpu.m_r[29],bits(result));throw rt::Fatal("scene original/native mismatch");}
    ++accepted;corrected+=memory.io.input.size()==14;cpu_steps+=steps;geometry_steps+=memory.geometry_steps;
}
}
int main(int argc,char** argv){try{
    if(argc!=3||std::fegetround()!=FE_TONEAREST)return 2;const auto program=read(argv[1]),firmware=read(argv[2]);
    if(program.size()!=2097152||firmware.size()!=6660)throw rt::Fatal("private size mismatch");
    rt::Tgp initial;Geometry startup;initial.reset();initial.bus=&startup;for(unsigned i=0;i<firmware.size()/4;++i)initial.prog[i]=word(firmware,4*i);
    startup.input={0x04000000};rt::tgpgen::run(initial,1000);if(initial.pc!=0x77||initial.count!=108||startup.output!=std::vector<std::uint32_t>{8})throw rt::Fatal("firmware startup mismatch");
    std::uint32_t seed=0x682ea7d3;const auto sample=[&](){seed=1664525u*seed+1013904223u;return value((seed&0x807fffffu)|((115u+(seed>>24)%20)<<23));};
    for(unsigned i=0;i<4096;++i){SceneContactGeometry input{{sample(),sample(),sample()},{sample(),sample(),sample()},{sample(),sample(),sample()}};compare(program,initial,input,(int(i%9)-4)*.25f);}
    for(auto damping:{-0.0f,0.0f,.5f,1.0f,2.0f})for(auto direction:{-1.0f,-0.0f,0.0f,1.0f})
        compare(program,initial,{{1,0,0},{0,0,0},{direction,0,0}},damping);
    // Used underflow/zero metric and nonfinite inputs are explicit rejections.
    compare(program,initial,{{0,1,1},{0,0,0},{0,1,0}},.5f);
    compare(program,initial,{{value(1),0,0},{0,0,0},{1,0,0}},.5f);
    compare(program,initial,{{1,0,0},{0,0,0},{value(0x7fc00000),0,0}},.5f);
    std::printf("SCENE_CONTACT_DAMPING_ORACLE PASS cases=%u accepted=%u corrected=%u rejected=%u cpu_instructions=%llu geometry_instructions=%llu hash=%016llx\n",total,accepted,corrected,rejected,(unsigned long long)cpu_steps,(unsigned long long)geometry_steps,(unsigned long long)hash);
}catch(const std::exception& error){std::fprintf(stderr,"scene oracle failed: %s\n",error.what());return 1;}return 0;}
