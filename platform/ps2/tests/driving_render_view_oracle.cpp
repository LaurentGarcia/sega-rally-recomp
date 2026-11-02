// Offline original-program oracle, excluded from every native target.
#include "driving_render_view.h"
#include "refcore/i960_ref.h"
#include "runtime/tgp.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>
namespace {
using namespace ps2::native;
using Bytes=std::vector<std::uint8_t>;
Bytes read(const char*path){std::ifstream f(path,std::ios::binary);if(!f)throw rt::Fatal("missing view oracle input");return {std::istreambuf_iterator<char>(f),{}};}
std::uint32_t word(const Bytes&b,unsigned p){return b.at(p)|(std::uint32_t(b.at(p+1))<<8)|(std::uint32_t(b.at(p+2))<<16)|(std::uint32_t(b.at(p+3))<<24);}
std::uint32_t bits(float x){return std::bit_cast<std::uint32_t>(x);}
float value(std::uint32_t x){return std::bit_cast<float>(x);}
using Matrix=std::array<std::uint32_t,12>;
Matrix words(const AffineTransform3&m){return {bits(m.columns[0].x),bits(m.columns[0].y),bits(m.columns[0].z),bits(m.columns[1].x),bits(m.columns[1].y),bits(m.columns[1].z),bits(m.columns[2].x),bits(m.columns[2].y),bits(m.columns[2].z),bits(m.translation.x),bits(m.translation.y),bits(m.translation.z)};}
struct Geometry:rt::TgpBus {
    std::vector<std::uint32_t> input,output;unsigned used=0;
    bool fifo_pop(std::uint32_t&v)override{if(used==input.size())return false;v=input[used++];return true;}
    void fifo_push(std::uint32_t v)override{output.push_back(v);}
    std::uint32_t mem_r(std::uint32_t)override{throw rt::TgpFatal("view external geometry read");}
    void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("view external geometry write");}
    void bank_w(std::uint32_t v)override{if(v)throw rt::TgpFatal("view bank changed");}
};
struct Memory:rt::Bus {
    const Bytes& image;rt::Tgp t;Geometry io;unsigned popped=0;std::map<std::uint32_t,std::uint8_t> data;
    std::vector<std::array<std::uint32_t,2>> presentation;
    explicit Memory(const Bytes&program,const rt::Tgp&initial):image(program),t(initial){t.bus=&io;}
    void set(std::uint32_t a,std::uint32_t v){for(unsigned i=0;i<4;++i)data[a+i]=v>>(8*i);}
    void flush(){const auto before=t.count;rt::tgpgen::run(t,t.count+10000);if(t.count-before>=10000||t.pc!=0x77||io.used!=io.input.size()||t.tables||t.bank_reg||t.sincos_base||t.inv_base||t.isqrt_base)throw rt::Fatal("view geometry completion differs");}
    std::uint8_t read_byte(std::uint32_t a)override{
        if(data.contains(a))return data.at(a);
        if(a>=0x59f000&&a<0x600000)return image.at(a-0x59f000);
        if(a>=0x220000&&a<0x224000)return 0;
        std::fprintf(stderr,"view undeclared read=%08x\n",a);throw rt::Fatal("view undeclared input");
    }
    std::uint16_t read_word(std::uint32_t a)override{return read_byte(a)|(std::uint16_t(read_byte(a+1))<<8);}
    std::uint32_t read_dword(std::uint32_t a)override{
        if(a==0x884000){flush();if(popped>=io.output.size())throw rt::Fatal("missing view geometry result");return io.output[popped++];}
        return read_word(a)|(std::uint32_t(read_word(a+2))<<16);
    }
    void write_byte(std::uint32_t a,std::uint8_t v)override{
        if(!data.contains(a)&&!(a>=0x220000&&a<0x224000)){std::fprintf(stderr,"view undeclared write=%08x\n",a);throw rt::Fatal("view undeclared output");}
        data[a]=v;
    }
    void write_word(std::uint32_t a,std::uint16_t v)override{write_byte(a,v);write_byte(a+1,v>>8);}
    void write_dword(std::uint32_t a,std::uint32_t v)override{
        if(a>=0x884000&&a<0x884040){io.input.push_back(v);return;}
        if(a==0x800090||a==0x8000a0||a==0x804000||a==0x804004||a==0x804008){presentation.push_back({a,v});return;}
        write_word(a,v);write_word(a+2,v>>16);
    }
    std::uint16_t flags(std::uint32_t a)override{return a>=0x884000&&a<0x884040?0:rt::Cpu::BURST;}
    std::uint32_t fetch(std::uint32_t a)override{
        const auto pc=a>=0x59f000?a-0x59f000:a;
        if((pc>=0x1ff60&&pc<0x202dc)||(pc>=0x27b28&&pc<0x27bc0)||
           (pc>=0x29120&&pc<0x292c4)||(pc>=0x33a50&&pc<0x33c00))return word(image,pc);
        std::fprintf(stderr,"view undeclared PC=%08x\n",pc);throw rt::Fatal("view instruction region differs");
    }
};
unsigned cases=0,live_cases=0;std::uint64_t instructions=0,fingerprint=14695981039346656037ull;
void compare(const Bytes&program,const rt::Tgp&initial,const RenderPose&pose,float old,float adjustment,const std::array<std::uint32_t,21>* captured=nullptr){
    DrivingRenderViewState state{old};DrivingRenderView out;
    if(!update_driving_render_view(pose,{adjustment},state,out))throw rt::Fatal("native view rejected bounded input");
    Memory memory(program,initial);memory.set(0x213980,0x213b40);
    const Matrix identity{0x3f800000,0,0,0,0x3f800000,0,0,0,0x3f800000,0,0,0};
    for(unsigned i=0;i<12;++i)memory.t.data[0x200+memory.t.data[7]+i]=identity[i];
    for(unsigned i=0;i<3;++i){memory.set(0x213b40+4*i,pose.words.position[i]);memory.set(0x213b58+4*i,pose.words.orientation[i]);}
    memory.set(0x20ab5c,bits(old));memory.set(0x20ac74,bits(adjustment));memory.set(0x2139d8,0);
    for(unsigned a=0x202200;a<0x202218;a+=4)memory.set(a,0);
    memory.set(0x202270,0);memory.set(0x202274,0);
    rt::I960Ref cpu(&memory);cpu.m_IP=0x5bef60;cpu.m_AC=0x3f001000;cpu.m_r[0]=0x223000;cpu.m_r[1]=0x220080;cpu.m_r[31]=0x220000;
    Matrix expected{};bool matrix_seen=false;unsigned steps=0;
    while(cpu.m_IP!=0x5bf2d8){
        if(++steps>1200)throw rt::Fatal("view CPU bound exceeded");
        if(cpu.m_IP==0x5bf1f0){for(unsigned i=0;i<12;++i)expected[i]=memory.read_dword(cpu.m_r[31]+0x60+4*i);matrix_seen=true;}
        cpu.execute_one();
    }
    memory.flush();const auto actual=words(out.world_to_view);
    if(!matrix_seen)throw rt::Fatal("view matrix checkpoint missing");
    for(unsigned i=0;i<12;++i)if(actual[i]!=expected[i]){std::fprintf(stderr,"VIEW MATRIX case=%u lane=%u native=%08x original=%08x\n",cases,i,actual[i],expected[i]);throw rt::Fatal("view matrix mismatch");}
    const std::array<std::uint32_t,9> native{bits(out.orientation.x),bits(out.orientation.y),bits(out.orientation.z),bits(out.position.x),bits(out.position.y),bits(out.position.z),bits(state.yaw_history),bits(out.focus_x),bits(out.focus_y)};
    std::array<std::uint32_t,9> original{};for(unsigned i=0;i<6;++i)original[i]=memory.read_dword(0x202200+4*i);original[6]=memory.read_dword(0x20ab5c);original[7]=memory.read_dword(0x202270);original[8]=memory.read_dword(0x202274);
    for(unsigned i=0;i<9;++i)if(native[i]!=original[i]){std::fprintf(stderr,"VIEW VALUE case=%u lane=%u native=%08x original=%08x\n",cases,i,native[i],original[i]);throw rt::Fatal("view value mismatch");}
    if(captured){
        for(unsigned i=0;i<12;++i)if(actual[i]!=(*captured)[i])throw rt::Fatal("captured view matrix mismatch");
        for(unsigned i=0;i<9;++i)if(native[i]!=(*captured)[12+i])throw rt::Fatal("captured view state mismatch");
        ++live_cases;
    }
    for(auto v:actual)for(unsigned i=0;i<4;++i){fingerprint^=(v>>(8*i))&255;fingerprint*=1099511628211ull;}
    instructions+=steps;++cases;
}
}
int main(int argc,char**argv){try{
    if((argc!=3&&argc!=4)||std::fegetround()!=FE_TONEAREST)return 2;
    const auto program=read(argv[1]),firmware=read(argv[2]);if(program.size()!=2097152||firmware.size()!=6660)return 2;
    rt::Tgp initial;initial.reset();Geometry startup;initial.bus=&startup;
    for(unsigned i=0;i<firmware.size()/4;++i)initial.prog[i]=word(firmware,4*i);
    startup.input={0x04000000};rt::tgpgen::run(initial,1000);
    if(initial.count!=108||initial.pc!=0x77||startup.output!=std::vector<std::uint32_t>{8})throw rt::Fatal("view startup differs");
    std::uint32_t seed=0x84213;const auto random=[&](float scale){seed=seed*1664525u+1013904223u;return float(std::int32_t(seed%65537)-32768)*scale;};
    for(unsigned n=0;n<1024;++n){RenderPose pose;for(auto&x:pose.words.position)x=bits(random(1.0f/32));for(auto&x:pose.words.orientation)x=bits(random(1.0f/16384));compare(program,initial,pose,random(1.0f/16384),random(1.0f/4096));}
    if(argc==4){
        const auto records=read(argv[3]);
        if(records.size()<12||std::string(records.begin(),records.begin()+8)!="RLYVIEW1")throw rt::Fatal("view records header");
        const auto count=word(records,8);if(!count||count>4096||records.size()!=12+180*count)throw rt::Fatal("view record extent");
        for(unsigned row=0;row<count;++row){const auto start=12+180*row;RenderPose pose;unsigned at=0;
            for(auto*field:{&pose.words.position,&pose.words.motion,&pose.words.orientation})for(auto&v:*field)v=word(records,start+4*at++);
            for(auto&v:pose.words.attributes)v=word(records,start+4*at++);
            pose.flags=word(records,start+4*at++);pose.progress=word(records,start+4*at++);
            const auto old=value(word(records,start+4*at++)),adjustment=value(word(records,start+4*at++));
            std::array<std::uint32_t,21> captured;for(auto&v:captured)v=word(records,start+4*at++);
            compare(program,initial,pose,old,adjustment,&captured);
        }
    }
    std::printf("DRIVING_RENDER_VIEW_ORACLE PASS cases=%u matrix_words=%u state_words=%u cpu_instructions=%llu hash=%016llx external_accesses=0 live_cases=%u\n",cases,cases*12,cases*9,(unsigned long long)instructions,(unsigned long long)fingerprint,live_cases);
}catch(const std::exception&e){std::fprintf(stderr,"view oracle: %s\n",e.what());return 1;}return 0;}
