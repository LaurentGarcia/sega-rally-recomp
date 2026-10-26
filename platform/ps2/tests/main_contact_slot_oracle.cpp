// Private original-program comparison, excluded from native applications.
#include "main_contact_slot_cases.h"
#include "refcore/i960_ref.h"
#include "runtime/tgp.h"
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
namespace {
namespace cases=main_slot_cases;using namespace ps2::native;
std::vector<std::uint8_t> read(const char*p){std::ifstream f(p,std::ios::binary);if(!f)throw rt::Fatal("missing slot oracle input");return {std::istreambuf_iterator<char>(f),{}};}
std::uint32_t word(const std::vector<std::uint8_t>&v,std::size_t n){return v.at(n)|(std::uint32_t(v.at(n+1))<<8)|(std::uint32_t(v.at(n+2))<<16)|(std::uint32_t(v.at(n+3))<<24);}
struct Geometry:rt::TgpBus{std::vector<std::uint32_t> input,output;unsigned consumed=0;bool fifo_pop(std::uint32_t&v)override{if(consumed==input.size())return false;v=input[consumed++];return true;}void fifo_push(std::uint32_t v)override{if(output.size()>=3)throw rt::TgpFatal("excess slot geometry output");output.push_back(v);}std::uint32_t mem_r(std::uint32_t)override{throw rt::TgpFatal("slot geometry external read");}void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("slot geometry external write");}void bank_w(std::uint32_t v)override{if(v)throw rt::TgpFatal("slot geometry bank change");}};
void constants(const rt::Tgp&t){
    constexpr std::array<unsigned,7> addresses{1,3,4,0x220,0x23a,0x23b,0x23c};
    constexpr std::array<std::uint32_t,7> expected{1,0,0x3f800000u,0x3f000000u,0x3f1714bau,0x3ed5a9a8u,0x3f3504f4u};
    for(unsigned i=0;i<addresses.size();++i)if(t.data[addresses[i]]!=expected[i])throw rt::TgpFatal("slot startup constants differ");
}
struct Done{};unsigned command=0;
void hook(rt::Tgp&t,std::uint16_t pc){const bool allowed=command==0x58?((pc>=0x51c&&pc<0x530)||(pc>=0x1ab&&pc<0x1cb)):(pc>=0x30e&&pc<0x327);if(!allowed||t.count>52)throw rt::TgpFatal("slot geometry escaped verified range");if(pc==(command==0x58?0x52f:0x326)){if(t.pc!=0xfff)throw rt::TgpFatal("slot geometry return differs");throw Done{};}}
constexpr std::uint32_t base=0x210000,frame=base,node=base+0x1000,vector=base+0x1200,triple=base+0x1220,axis=base+0x1240,response=base+0x1260;
struct Memory:rt::Bus{
    const std::vector<std::uint8_t>&image;rt::Tgp tgp;std::array<std::uint32_t,0x5000/4>ram{};std::vector<std::uint32_t>fifo;unsigned kind=0,index=0,commands=0;std::uint64_t geometry_steps=0;
    Memory(const std::vector<std::uint8_t>&p,const rt::Tgp&t):image(p),tgp(t){for(unsigned i=0;i<ram.size();++i)ram[i]=0xa5190000u^i;}
    std::uint32_t&at(std::uint32_t a){if(a<base||a>=base+ram.size()*4||a&3)throw rt::Fatal("slot memory range/alignment");return ram[(a-base)/4];}
    bool readable(std::uint32_t a)const{return(a>=frame+0x40&&a<=frame+0xb0)||(a>=vector&&a<vector+12)||a==triple+4||a==axis||a==axis+8||a==node+0x50||(a>=node+0x64&&a<=node+0x70)||(a>=0x2138d0&&a<0x213960)||a==0x214208||(a>=0x2141d0&&a<=0x2141e0)||a==0x214124;}
    bool writable(std::uint32_t a)const{return(a>=frame+0x40&&a<=frame+0xb0)||(a>=response&&a<response+12)||(a>=node+0x64&&a<=node+0x70)||(a>=0x213880&&a<0x2138c0)||a==0x214208;}
    std::uint32_t fetch(std::uint32_t a)override{if(a<0x5d6200||a>=0x5d6678||a&3)throw rt::Fatal("slot instruction range");return word(image,a-0x59f000);}
    std::uint32_t read_dword(std::uint32_t a)override{if(a==0x884000){if(kind||index>=fifo.size())throw rt::Fatal("slot FIFO read");return fifo[index++];}if(!readable(a))throw rt::Fatal("unexpected slot word read");return at(a);}
    void write_dword(std::uint32_t a,std::uint32_t v)override{if(a==0x884000){
        if(!kind){if(index!=fifo.size()||(v!=0x2c005858u&&v!=0x17802f2fu))throw rt::Fatal("slot unexpected geometry command");kind=v==0x2c005858u?0x58:0x2f;fifo.clear();index=0;return;}
        fifo.push_back(v);if(fifo.size()==(kind==0x58?6u:3u)){Geometry bus;bus.input=fifo;tgp.bus=&bus;tgp.pc=kind==0x58?0x51c:0x30e;tgp.pcs[0]=0xfff;tgp.count=0;tgp.hook=hook;command=kind;
            try{rt::tgpgen::run(tgp,53);throw rt::Fatal("slot geometry instruction bound");}catch(Done&){}
            if(bus.consumed!=fifo.size()||bus.output.size()!=(kind==0x58?1u:3u)||tgp.bank_reg)throw rt::Fatal("slot geometry transfer count");
            if((kind==0x2f&&tgp.count!=25)||(kind==0x58&&tgp.count!=24&&tgp.count!=49&&tgp.count!=52))throw rt::TgpFatal("slot geometry count differs");constants(tgp);
            geometry_steps+=tgp.count;++commands;fifo=bus.output;kind=0;}return;}
        if(!writable(a))throw rt::Fatal("unexpected slot word write");at(a)=v;}
    std::uint8_t read_byte(std::uint32_t a)override{if(a!=frame+0x5c)throw rt::Fatal("slot byte read");return std::uint8_t(at(a));}
    std::uint16_t read_word(std::uint32_t a)override{if(a!=node+0x12)throw rt::Fatal("slot halfword read");return std::uint16_t(at(a&~3u)>>16);}
    void write_word(std::uint32_t a,std::uint16_t v)override{if(a!=node+0x12)throw rt::Fatal("slot halfword write");at(a&~3u)=(at(a&~3u)&65535u)|(std::uint32_t(v)<<16);}
    void write_byte(std::uint32_t,std::uint8_t)override{throw rt::Fatal("unexpected slot byte write");}
    std::uint16_t flags(std::uint32_t)override{return rt::Cpu::BURST;}
};
unsigned count=0,active=0,live=0;std::uint64_t hash=14695981039346656037ull,steps=0,geometry=0;
void compare(const std::vector<std::uint8_t>&program,const rt::Tgp&initial,const cases::Case&c,const cases::Words*captured=nullptr){
    auto state=c.state;MainContactSlotOutput out;if(!update_main_contact_slot(c.input,state,out)){std::fprintf(stderr,"REJECT case=%u live=%u slot=%u gap=%08x vector=%08x,%08x,%08x\n",count,live,unsigned(c.input.slot),cases::bits(c.input.contact.gap),cases::bits(c.input.vector.x),cases::bits(c.input.vector.y),cases::bits(c.input.vector.z));throw rt::Fatal("bounded slot input rejected");}
    constants(initial);Memory m(program,initial);const unsigned slot=c.input.slot;const auto result=0x2138d0+36*slot;
    m.at(node+0x10)=0xa719u|(std::uint32_t(c.state.packed_types)<<16);m.at(node+0x50)=cases::bits(c.input.object_parameter);
    for(unsigned i=0;i<4;++i){m.at(node+0x64+4*i)=cases::bits(c.state.history[i]);m.at(0x2138a0+4*i)=cases::bits(c.state.published_middle[i]);m.at(0x2138b0+4*i)=cases::bits(c.state.gain[i]);m.at(0x213890+4*i)=cases::bits(c.state.metric[i]);m.at(0x213880+4*i)=cases::bits(c.state.lateral[i]);}
    m.at(0x214208)=c.state.type_mask;for(unsigned i=0;i<5;++i)m.at(0x2141d0+4*i)=cases::bits(c.input.parameters[i]);m.at(0x214124)=cases::bits(c.input.control);
    for(unsigned i=0;i<3;++i)m.at(result+12+4*i)=cases::bits(c.input.contact.direction[i]);m.at(result+24)=cases::bits(c.input.contact.gap);m.at(result+28)=c.input.contact.status;
    m.at(vector)=cases::bits(c.input.vector.x);m.at(vector+4)=cases::bits(c.input.vector.y);m.at(vector+8)=cases::bits(c.input.vector.z);m.at(triple+4)=cases::bits(c.input.reference_middle);m.at(axis)=cases::bits(c.input.axis_direction.x);m.at(axis+8)=cases::bits(c.input.axis_direction.z);
    const auto before=m.ram;rt::I960Ref cpu(&m);for(auto&r:cpu.m_r)r=0x51a35713;cpu.m_AC=0x3f001000;cpu.m_IP=0x5d6200;cpu.m_r[1]=frame+0x40;cpu.m_r[31]=frame;
    cpu.m_r[16]=node;cpu.m_r[17]=vector;cpu.m_r[18]=triple;cpu.m_r[19]=axis;cpu.m_r[20]=cases::bits(c.input.factor);cpu.m_r[21]=response;cpu.m_r[22]=slot;cpu.m_r[30]=0;
    unsigned instructions=0;while(cpu.m_IP!=0x5d6360&&cpu.m_IP!=0x5d6674){if(++instructions>260)throw rt::Fatal("slot CPU instruction bound");cpu.execute_one();}
    MainContactSlotState observed;observed.packed_types=std::uint16_t(m.at(node+0x10)>>16);observed.type_mask=m.at(0x214208);
    for(unsigned i=0;i<4;++i){observed.history[i]=cases::value(m.at(node+0x64+4*i));observed.published_middle[i]=cases::value(m.at(0x2138a0+4*i));observed.gain[i]=cases::value(m.at(0x2138b0+4*i));observed.metric[i]=cases::value(m.at(0x213890+4*i));observed.lateral[i]=cases::value(m.at(0x213880+4*i));}
    MainContactSlotOutput actual{{cases::value(m.at(response)),cases::value(m.at(response+4)),cases::value(m.at(response+8))},bool(cpu.m_r[16])};
    const auto wanted=cases::words(state,out),got=cases::words(observed,actual);
    if(got!=wanted||(captured&&got!=*captured)||cpu.m_r[16]>1||m.kind||m.index!=m.fifo.size()){
        for(unsigned i=0;i<got.size();++i)if(got[i]!=wanted[i])std::fprintf(stderr,"SLOT case=%u slot=%u field=%u expected=%08x native=%08x\n",count,slot,i,got[i],wanted[i]);throw rt::Fatal("original/native/captured slot mismatch");}
    for(unsigned i=0;i<m.ram.size();++i){const auto address=base+4*i;if(!m.writable(address)&&address!=node+0x10&&m.ram[i]!=before[i])throw rt::Fatal("slot modified immutable data");}
    if((m.at(node+0x10)&65535u)!=0xa719u||m.commands!=(out.active?((cases::bits(state.metric[slot])&0x7fffffffu)?2u:1u):0u))throw rt::Fatal("slot halfword/command preservation");
    for(unsigned i=24;i<=28;++i)if(cpu.m_r[i]!=0x51a35713)throw rt::Fatal("slot preserved register differs");
    if(cpu.m_r[30]||cpu.m_IP!=(out.active?0x5d6674u:0x5d6360u))throw rt::Fatal("slot return or zero register differs");
    cases::hash(hash,got);++count;active+=out.active;live+=captured!=nullptr;steps+=instructions;geometry+=m.geometry_steps;
}
}
int main(int argc,char**argv){try{
    if(argc!=3&&argc!=5)return 2;if(std::fegetround()!=FE_TONEAREST)throw rt::Fatal("slot reference rounding mode");const auto program=read(argv[1]),firmware=read(argv[2]);if(program.size()!=2097152||firmware.size()!=6660)throw rt::Fatal("slot image size");
    rt::Tgp initial;Geometry startup;initial.reset();initial.bus=&startup;for(unsigned i=0;i<firmware.size()/4;++i)initial.prog[i]=word(firmware,4*i);startup.input={0x04000000};rt::tgpgen::run(initial,1000);if(initial.count!=108||startup.output!=std::vector<std::uint32_t>{8})throw rt::Fatal("slot firmware startup");
    const auto table=cases::table();std::array<std::uint32_t,65536>raw{};for(unsigned i=0;i<table.size();++i){raw[0xc000+2*i]=cases::bits(table[i].slope);raw[0xc001+2*i]=cases::bits(table[i].intercept);}initial.tables=raw.data();
    if(!cases::each([&](auto c){c.input.normal_table=table;compare(program,initial,c);return true;},true))return 1;

    if(count!=25852||active!=7248||hash!=0x121b47e419609ea7ull)throw rt::Fatal("slot synthetic fingerprint differs");
    if(argc==5){
        const auto tables=read(argv[3]),records=read(argv[4]);
        if(tables.size()!=262144||records.size()<12||std::string(records.begin(),records.begin()+8)!="MCSLOT01")throw rt::Fatal("slot private record header");
        const auto rows=word(records,8);if(!rows||rows>128||records.size()!=12+rows*69u*4u)throw rt::Fatal("slot private record bounds");
        std::array<ContactNormalPair,8192> owned_table;
        for(unsigned i=0;i<raw.size();++i)raw[i]=word(tables,4*i);
        for(unsigned i=0;i<owned_table.size();++i)owned_table[i]={cases::value(raw[0xc000+2*i]&0x7fffffffu),cases::value(raw[0xc001+2*i])};
        for(unsigned row=0;row<rows;++row){
            unsigned cursor=12+row*69*4;auto next=[&](){const auto v=word(records,cursor);cursor+=4;return v;};
            cases::Case c;const auto slot=next();if(slot>=4)throw rt::Fatal("slot record index");c.input.slot=std::uint8_t(slot);c.input.contact.status=next();c.input.contact.gap=cases::value(next());
            for(auto&v:c.input.contact.direction)v=cases::value(next());
            c.input.vector={cases::value(next()),cases::value(next()),cases::value(next())};
            c.input.axis_direction={cases::value(next()),cases::value(next()),cases::value(next())};
            c.input.reference_middle=cases::value(next());c.input.factor=cases::value(next());c.input.object_parameter=cases::value(next());
            for(auto&v:c.input.parameters)v=cases::value(next());c.input.control=cases::value(next());
            const auto flags=next();if(flags>65535)throw rt::Fatal("slot record flags");c.state.packed_types=std::uint16_t(flags);c.state.type_mask=next();
            for(auto*array:{&c.state.history,&c.state.published_middle,&c.state.gain,&c.state.metric,&c.state.lateral})for(auto&v:*array)v=cases::value(next());
            cases::Words captured;for(auto&v:captured)v=next();c.input.normal_table=owned_table;compare(program,initial,c,&captured);
        }
    }
    std::printf("MAIN_CONTACT_SLOT_ORACLE PASS cases=%u active=%u live=%u cpu_instructions=%llu geometry_instructions=%llu hash=%016llx\n",count,active,live,(unsigned long long)steps,(unsigned long long)geometry,(unsigned long long)hash);
}catch(const std::exception&e){std::fprintf(stderr,"slot oracle failed: %s\n",e.what());return 1;}return 0;}
