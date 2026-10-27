// Private original-program comparison; excluded from native applications.
#include "main_balance_geometry_cases.h"
#include "refcore/i960_ref.h"
#include "runtime/tgp.h"
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
namespace {
namespace cases=main_geometry_cases;
using Bytes=std::vector<std::uint8_t>;
Bytes read(const char*p){std::ifstream f(p,std::ios::binary);if(!f)throw rt::Fatal("missing geometry oracle input");return {std::istreambuf_iterator<char>(f),{}};}
std::uint32_t word(const Bytes&v,std::size_t n){return v.at(n)|(std::uint32_t(v.at(n+1))<<8)|(std::uint32_t(v.at(n+2))<<16)|(std::uint32_t(v.at(n+3))<<24);}
struct Geometry:rt::TgpBus {
    std::vector<std::uint32_t> input,output;unsigned consumed=0;
    bool fifo_pop(std::uint32_t&v)override{if(consumed==input.size())return false;v=input[consumed++];return true;}
    void fifo_push(std::uint32_t v)override{if(output.size()>=6)throw rt::TgpFatal("excess balance geometry output");output.push_back(v);}
    std::uint32_t mem_r(std::uint32_t)override{throw rt::TgpFatal("balance geometry external read");}
    void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("balance geometry external write");}
    void bank_w(std::uint32_t v)override{if(v)throw rt::TgpFatal("balance geometry bank change");}
};
std::uint64_t hooks=0;
void hook(rt::Tgp&t,std::uint16_t pc){
    const bool dispatch=pc==0xcf||pc==0xd0||pc==0xd4||pc==0xd8||pc==0xd9||pc==0xda||pc==0xf1||pc==0xf2||pc==0xf3||pc==0xf7;
    const bool allowed=dispatch||(pc>=0x77&&pc<=0xae)||(pc>=0x1e6&&pc<=0x21c)||(pc>=0x25b&&pc<=0x27d)||(pc>=0x327&&pc<=0x32d)||(pc>=0x384&&pc<=0x39f)||(pc>=0x404&&pc<=0x421)||(pc>=0x44c&&pc<=0x46b);
    if(!allowed){std::fprintf(stderr,"BALANCE_GEOMETRY unexpectedPC=%x\n",pc);throw rt::TgpFatal("balance geometry firmware range");}
    if(t.tables||t.bank_reg||t.sincos_base||t.inv_base||t.isqrt_base)throw rt::TgpFatal("balance geometry table dependency");++hooks;
}
constexpr std::uint32_t base=0x210000,frame=base,node=base+0x1000,first=base+0x1200,second=base+0x1220,published=0x214264;
struct Memory:rt::Bus {
    const Bytes&image;rt::Tgp tgp;Geometry geometry;unsigned output_index=0;
    std::array<std::uint32_t,0x5000/4>ram{};
    Memory(const Bytes&p,const rt::Tgp&t):image(p),tgp(t){tgp.bus=&geometry;tgp.hook=hook;for(unsigned i=0;i<ram.size();++i)ram[i]=0xa5310000u^i;}
    std::uint32_t&at(std::uint32_t a){if(a<base||a>=base+4*ram.size()||a&3)throw rt::Fatal("balance geometry memory range");return ram[(a-base)/4];}
    bool writable(std::uint32_t a)const{return (a>=frame+0x40&&a<=frame+0x64)||(a>=first&&a<first+12)||(a>=second&&a<second+12)||a==published;}
    void flush(){const auto count=tgp.count,before=hooks;rt::tgpgen::run(tgp,2048);if(tgp.count-count>=2048||hooks-before!=tgp.count-count||geometry.consumed!=geometry.input.size()||tgp.pc!=0x77)throw rt::Fatal("balance geometry FIFO completion");}
    std::uint32_t fetch(std::uint32_t a)override{if(a<0x5d72f8||a>=0x5d7658||a&3)throw rt::Fatal("balance geometry instruction range");return word(image,a-0x59f000);}
    std::uint32_t read_dword(std::uint32_t a)override{if(a==0x884000){flush();if(output_index>=geometry.output.size())throw rt::Fatal("balance geometry missing output");return geometry.output[output_index++];}if(!writable(a)&&a!=node+0x3c&&a!=node+0x74)throw rt::Fatal("balance geometry unexpected read");return at(a);}
    void write_dword(std::uint32_t a,std::uint32_t v)override{if(a==0x884000){if(geometry.input.size()>=31)throw rt::Fatal("balance geometry excessive input");geometry.input.push_back(v);return;}if(!writable(a))throw rt::Fatal("balance geometry unexpected write");at(a)=v;}
    std::uint8_t read_byte(std::uint32_t)override{throw rt::Fatal("balance geometry byte read");}
    std::uint16_t read_word(std::uint32_t)override{throw rt::Fatal("balance geometry halfword read");}
    void write_byte(std::uint32_t,std::uint8_t)override{throw rt::Fatal("balance geometry byte write");}
    void write_word(std::uint32_t,std::uint16_t)override{throw rt::Fatal("balance geometry halfword write");}
    std::uint16_t flags(std::uint32_t)override{return rt::Cpu::BURST;}
};
unsigned count=0,live=0;std::uint64_t cpu_steps=0,tgp_steps=0,hash=14695981039346656037ull;
void compare(const Bytes&program,const rt::Tgp&initial,const cases::Case&c,const cases::Output*captured=nullptr){
    auto result=c.state;if(!ps2::native::publish_main_balance_geometry(c.input,result))throw rt::Fatal("bounded balance geometry rejected");
    Memory m(program,initial);const auto input=cases::words(c);
    m.at(node+0x74)=input[6];m.at(node+0x3c)=input[7];m.at(published)=input[15];
    for(unsigned i=0;i<3;++i){m.at(first+4*i)=input[9+i];m.at(second+4*i)=input[12+i];}
    const auto memory_before=m.ram;const unsigned oldbase=m.tgp.data[7],olddepth=m.tgp.data[9];std::array<std::uint32_t,12>saved;
    for(unsigned i=0;i<12;++i){saved[i]=0x3f000000u+((i+count)&0x7fffffu);m.tgp.data[0x200+oldbase+i]=saved[i];}
    rt::I960Ref cpu(&m);cpu.m_AC=0x3f001000;cpu.m_IP=0x5d72f8;cpu.m_r[31]=frame;cpu.m_r[30]=0;
    cpu.m_r[3]=first;cpu.m_r[15]=second;cpu.m_r[9]=node;cpu.m_r[10]=input[8];
    cpu.m_r[6]=input[0];cpu.m_r[8]=input[1];cpu.m_r[5]=input[2];cpu.m_r[4]=input[3];cpu.m_r[14]=input[4];cpu.m_r[13]=input[5];
    const auto start=m.tgp.count;unsigned steps=0;
    while(cpu.m_IP!=0x5d7658){if(++steps>180)throw rt::Fatal("balance geometry CPU bound");cpu.execute_one();}
    m.flush();cases::Output observed;for(unsigned i=0;i<3;++i){observed[i]=m.at(first+4*i);observed[3+i]=m.at(second+4*i);}observed[6]=m.at(published);
    const auto native=cases::words(result);
    if(native!=observed||(captured&&native!=*captured)){for(unsigned i=0;i<7;++i)if(native[i]!=observed[i])std::fprintf(stderr,"BALANCE_GEOMETRY case=%u field=%u native=%08x original=%08x\n",count,i,native[i],observed[i]);throw rt::Fatal("balance geometry original/native/capture differs");}
    if(m.geometry.input.size()!=31||m.output_index!=6||m.geometry.output.size()!=6||m.tgp.data[7]!=oldbase||m.tgp.data[9]!=olddepth||cpu.m_r[30])throw rt::Fatal("balance geometry command/stack boundary");
    for(unsigned i=0;i<12;++i)if(m.tgp.data[0x200+oldbase+i]!=saved[i])throw rt::Fatal("balance geometry changed saved matrix");
    for(unsigned i=0;i<m.ram.size();++i)if(!m.writable(base+4*i)&&m.ram[i]!=memory_before[i])throw rt::Fatal("balance geometry immutable state changed");
    cases::hash(hash,input);cases::hash(hash,native);++count;live+=captured!=nullptr;cpu_steps+=steps;tgp_steps+=m.tgp.count-start;
}
}
int main(int argc,char**argv){try{
    if(argc!=3&&argc!=4)return 2;if(std::fegetround()!=FE_TONEAREST)throw rt::Fatal("balance geometry reference rounding");
    const auto program=read(argv[1]),firmware=read(argv[2]);if(program.size()!=2097152||firmware.size()!=6660)throw rt::Fatal("balance geometry image size");
    rt::Tgp initial;Geometry startup;initial.reset();initial.bus=&startup;for(unsigned i=0;i<firmware.size()/4;++i)initial.prog[i]=word(firmware,4*i);
    startup.input={0x04000000};rt::tgpgen::run(initial,1000);if(initial.count!=108||initial.pc!=0x77||startup.output!=std::vector<std::uint32_t>{8}||initial.tables)throw rt::Fatal("balance geometry startup");
    if(!cases::each([&](const auto&c){compare(program,initial,c);return true;},true))return 1;
    if(count!=20480||hash!=0x56b32fd92694a6a0ull||cpu_steps!=3173890||tgp_steps!=13783767)throw rt::Fatal("balance geometry synthetic coverage differs");
    if(argc==4){const auto records=read(argv[3]);if(records.size()<12||std::string(records.begin(),records.begin()+8)!="MBGEOM01")throw rt::Fatal("balance geometry records");const auto n=word(records,8);if(!n||n>32||records.size()!=12+n*23u*4u)throw rt::Fatal("balance geometry record count");for(unsigned i=0;i<n;++i){cases::Input input;cases::Output expected;for(unsigned j=0;j<16;++j)input[j]=word(records,12+92*i+4*j);for(unsigned j=0;j<7;++j)expected[j]=word(records,12+92*i+64+4*j);compare(program,initial,cases::input(input),&expected);}}
    std::printf("MAIN_BALANCE_GEOMETRY_ORACLE PASS cases=%u live=%u cpu_instructions=%llu geometry_instructions=%llu hash=%016llx\n",count,live,(unsigned long long)cpu_steps,(unsigned long long)tgp_steps,(unsigned long long)hash);
}catch(const std::exception&e){std::fprintf(stderr,"balance geometry oracle failed: %s\n",e.what());return 1;}return 0;}
