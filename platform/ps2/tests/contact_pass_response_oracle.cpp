// Offline composition comparison. Original query calls are explicit input
// boundaries; no original processor or firmware is linked into native builds.
#include "contact_pass_response_cases.h"
#include "refcore/i960_ref.h"
#include "runtime/tgp.h"
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
namespace {
namespace cases=contact_pass_response_cases;
using namespace ps2::native;
std::vector<std::uint8_t> read(const char* path){std::ifstream f(path,std::ios::binary);if(!f)throw rt::Fatal("missing private input");return {std::istreambuf_iterator<char>(f),{}};}
std::uint32_t word(const std::vector<std::uint8_t>& b,std::size_t o){return b.at(o)|(std::uint32_t(b.at(o+1))<<8)|(std::uint32_t(b.at(o+2))<<16)|(std::uint32_t(b.at(o+3))<<24);}
struct Geometry final:rt::TgpBus {
    std::vector<std::uint32_t> input,output;unsigned consumed=0;
    bool fifo_pop(std::uint32_t& v)override{if(consumed==input.size())return false;v=input[consumed++];return true;}
    void fifo_push(std::uint32_t v)override{if(output.size()>=3)throw rt::TgpFatal("excess geometry output");output.push_back(v);}
    std::uint32_t mem_r(std::uint32_t)override{throw rt::TgpFatal("unexpected geometry memory read");}
    void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("unexpected geometry memory write");}
    void bank_w(std::uint32_t v)override{if(v)throw rt::TgpFatal("unexpected geometry bank");}
};
struct Done{};unsigned command_kind=0;
void hook(rt::Tgp& t,std::uint16_t pc){
    const bool allowed=command_kind==0x5b?(pc>=0x55f&&pc<0x577):((pc>=0x51c&&pc<0x530)||(pc>=0x1ab&&pc<0x1cb));
    if(!allowed||t.count>52)throw rt::TgpFatal("composition geometry escaped verified region");
    if(pc==(command_kind==0x5b?0x576:0x52f)){if(t.pc!=0xfff)throw rt::TgpFatal("wrong geometry return");throw Done{};}
}
constexpr std::uint32_t base=0x210000,frame=base,node=base+0x1000,position=base+0x1200,orientation=base+0x1240,slots=0x2138d0;
struct Memory final:rt::Bus {
    const std::vector<std::uint8_t>& image;rt::Tgp tgp;
    std::array<std::uint32_t,0x5000/4> ram{};std::vector<std::uint32_t> fifo;
    unsigned kind=0,read_index=0,commands=0;std::uint64_t geometry_instructions=0;
    Memory(const std::vector<std::uint8_t>& p,const rt::Tgp& initial):image(p),tgp(initial){for(unsigned i=0;i<ram.size();++i)ram[i]=0xa5310000u^i;}
    std::uint32_t& at(std::uint32_t a){if(a<base||a>=base+ram.size()*4||(a&3))throw rt::Fatal("composition memory escaped bounded region");return ram[(a-base)/4];}
    bool readable(std::uint32_t a)const{
        return (a>=frame+0x40&&a<=frame+0x110)||(a>=node+0x14&&a<=node+0x58)||
            (a>=position&&a<position+12)||(a>=orientation&&a<orientation+12)||
            (a>=slots&&a<slots+144)||(a>=0x213960&&a<=0x213968)||a==0x2142c8;
    }
    bool writable(std::uint32_t a)const{
        return (a>=frame+0xa0&&a<=frame+0x110)||a==frame+0x74||a==frame+0x80||a==frame+0x8c||a==frame+0x98||
            a==node+0x2c||a==node+0x34||a==node+0x48||a==position||a==position+8||a==orientation+4||
            a==0x213960||a==0x213964||a==0x213968;
    }
    std::uint32_t fetch(std::uint32_t a)override{
        if(a<0x5d36fc||a>=0x5d3cc0||(a&3))throw rt::Fatal("composition instruction fetch escaped response region");
        return word(image,a-0x59f000);
    }
    std::uint32_t read_dword(std::uint32_t a)override{
        if(a==0x884000){if(kind||read_index>=fifo.size())throw rt::Fatal("unexpected geometry result read");return fifo[read_index++];}
        if(!readable(a))throw rt::Fatal("unexpected composition memory read");return at(a);
    }
    void write_dword(std::uint32_t a,std::uint32_t v)override{
        if(a==0x884000){
            if(!kind){
                if(read_index!=fifo.size()||(v!=0x2d805b5bu&&v!=0x2c005858u))throw rt::Fatal("unexpected composition geometry command");
                kind=v==0x2d805b5bu?0x5b:0x58;fifo.clear();read_index=0;return;
            }
            fifo.push_back(v);
            if(fifo.size()==6){
                Geometry bus;bus.input=fifo;tgp.bus=&bus;tgp.pc=kind==0x5b?0x55f:0x51c;tgp.pcs[0]=0xfff;tgp.count=0;tgp.hook=hook;command_kind=kind;
                try{rt::tgpgen::run(tgp,53);throw rt::Fatal("composition geometry did not return");}catch(Done&){}
                if(bus.consumed!=6||bus.output.size()!=(kind==0x5b?3u:1u)||tgp.tables||tgp.bank_reg)
                    throw rt::Fatal("composition geometry transfer counts differ");
                geometry_instructions+=tgp.count;++commands;fifo=bus.output;kind=0;
            }
            return;
        }
        if(!writable(a))throw rt::Fatal("unexpected composition memory write");at(a)=v;
    }
    std::uint8_t read_byte(std::uint32_t a)override{
        if(a<slots||a>=slots+144||(a-slots)%36!=28)throw rt::Fatal("unexpected composition byte read");return std::uint8_t(at(a));
    }
    std::uint16_t read_word(std::uint32_t)override{throw rt::Fatal("unexpected halfword read");}
    void write_byte(std::uint32_t,std::uint8_t)override{throw rt::Fatal("unexpected byte write");}
    void write_word(std::uint32_t,std::uint16_t)override{throw rt::Fatal("unexpected halfword write");}
    std::uint16_t flags(std::uint32_t)override{return rt::Cpu::BURST;}
};
unsigned count=0,live=0;std::uint64_t fingerprint=14695981039346656037ull,instructions=0,geometry_instructions=0;
void compare(const std::vector<std::uint8_t>& program,const rt::Tgp& initial,const cases::Case& c,const cases::Words* captured=nullptr){
    auto native=c.state;if(!resolve_contact_pass_response(c.input,native))throw rt::Fatal("composition input rejected by native domain");
    Memory memory(program,initial);
    for(unsigned i=0;i<4;++i){
        const auto& contact=c.input.contacts[i];
        for(unsigned j=0;j<3;++j)memory.at(slots+36*i+12+4*j)=cases::bits(contact.direction[j]);
        memory.at(slots+36*i+24)=cases::bits(contact.gap);memory.at(slots+36*i+28)=contact.status;
        const auto& v=c.input.offsets[i];memory.at(frame+0x70+12*i)=cases::bits(v.x);memory.at(frame+0x74+12*i)=cases::bits(v.y);memory.at(frame+0x78+12*i)=cases::bits(v.z);
    }
    const auto before=cases::words(c.state);
    for(unsigned i=0;i<3;++i){memory.at(node+std::array<unsigned,3>{0x14,0x1c,0x3c}[i])=cases::bits(c.input.bases[i]);memory.at(node+std::array<unsigned,3>{0x2c,0x34,0x48}[i])=before[i];memory.at(position+4*i)=before[3+i];memory.at(orientation+4*i)=before[6+i];}
    memory.at(node+0x50)=cases::bits(c.input.response_parameter);memory.at(node+0x54)=cases::bits(c.input.gain_parameter);memory.at(node+0x58)=cases::bits(c.input.scalar_divisor);
    memory.at(frame+0xf0)=orientation;memory.at(frame+0x100)=position;memory.at(0x213960)=0;memory.at(0x213964)=before[9];memory.at(0x213968)=before[10];memory.at(0x2142c8)=c.input.feedback_enabled?0:1;
    const auto initial_ram=memory.ram;
    rt::I960Ref cpu(&memory);cpu.m_AC=0x3f001000u;cpu.m_IP=0x5d3728;
    cpu.m_r[31]=frame;cpu.m_r[13]=node;cpu.m_r[3]=slots+28;cpu.m_r[9]=slots;cpu.m_r[10]=slots+24;
    cpu.m_r[7]=frame+0xd0;cpu.m_r[14]=frame+0xe0;cpu.m_r[25]=frame+0xd8;
    unsigned steps=0,adapters=0,queries=0;
    while(cpu.m_IP!=0x5d3cc0){
        if(++steps>1200)throw rt::Fatal("composition instruction bound exceeded");
        // The selected interpreted contacts are supplied by this oracle's
        // contract. Query and adapter calls are deliberately excluded; their
        // return boundaries resume the original loop with the supplied values.
        if(cpu.m_IP==0x5d370c){const auto index=adapters+1;if(index>3||cpu.m_r[16]!=frame+0x40+12*index||cpu.m_r[17]!=slots+36*index||cpu.m_r[18]!=0)throw rt::Fatal("interpreted-contact boundary arguments differ");++adapters;cpu.m_IP+=4;continue;}
        if(cpu.m_IP==0x5d3724){if(queries>=2||cpu.m_r[18]!=frame+0x40+12*(queries+2)||cpu.m_r[19]!=0)throw rt::Fatal("excluded next-query boundary arguments differ");++queries;cpu.m_IP+=4;continue;}
        cpu.execute_one();
    }
    cases::Words actual{};for(unsigned i=0;i<3;++i){actual[i]=memory.at(node+std::array<unsigned,3>{0x2c,0x34,0x48}[i]);actual[3+i]=memory.at(position+4*i);actual[6+i]=memory.at(orientation+4*i);}
    actual[9]=memory.at(0x213964);actual[10]=memory.at(0x213968);actual[11]=memory.at(0x213960);actual[12]=cpu.m_r[12];
    if(actual!=cases::words(native)||(captured&&actual!=*captured)||adapters!=3||queries!=2||memory.kind||memory.read_index!=memory.fifo.size()||
       memory.commands!=((actual[12]&0x7fffffffu)?8u:0u)){
        std::fprintf(stderr,"COMPOSITION MISMATCH case=%u calls=%u,%u commands=%u\n",count,adapters,queries,memory.commands);
        for(unsigned i=0;i<actual.size();++i)if(actual[i]!=cases::words(native)[i])std::fprintf(stderr,"field%u original=%08x native=%08x\n",i,actual[i],cases::words(native)[i]);
        throw rt::Fatal("composition original/native/captured output mismatch");
    }
    for(unsigned i=0;i<memory.ram.size();++i)if(!memory.writable(base+4*i)&&memory.ram[i]!=initial_ram[i])throw rt::Fatal("composition changed immutable storage");
    cases::hash(fingerprint,actual);++count;live+=captured!=nullptr;instructions+=steps-adapters-queries;geometry_instructions+=memory.geometry_instructions;
}
}
int main(int argc,char**argv){try{
    if(argc!=3&&argc!=4)return 2;
    if(std::fegetround()!=FE_TONEAREST)throw rt::Fatal("nearest-even host mode required");
    const auto program=read(argv[1]),firmware=read(argv[2]);if(program.size()!=2097152||firmware.size()!=6660)throw rt::Fatal("wrong private image size");
    rt::Tgp initial;Geometry startup;initial.reset();initial.bus=&startup;for(unsigned i=0;i<firmware.size()/4;++i)initial.prog[i]=word(firmware,4*i);
    startup.input={0x04000000};rt::tgpgen::run(initial,100000);initial.bus=nullptr;
    if(initial.count>=100000||startup.consumed!=1||startup.output!=std::vector<std::uint32_t>{8}||initial.tables||initial.bank_reg)throw rt::Fatal("composition firmware startup failed");
    if(!cases::each([&](const auto& c){compare(program,initial,c);return true;},true))return 1;
    if(count!=4224||fingerprint!=0xf99dcd3347d0f255ull)throw rt::Fatal("composition synthetic coverage changed");
    if(argc==4){
        const auto records=read(argv[3]);const std::array<std::uint8_t,8> magic{'R','X','P','A','S','S','1',0};
        if(records.size()<12||!std::equal(magic.begin(),magic.end(),records.begin()))throw rt::Fatal("invalid response-pass records");
        const auto n=word(records,8);if(!n||n>32||records.size()!=12ull+260ull*n)throw rt::Fatal("invalid response-pass record length");
        for(unsigned i=0;i<n;++i){
            const auto raw=[&](unsigned j){return word(records,12+260*i+4*j);};
            const auto value=[&](unsigned j){return cases::value(raw(j));};cases::Case c{};
            for(unsigned j=0;j<4;++j){c.input.contacts[j]={{value(5*j),value(5*j+1),value(5*j+2)},value(5*j+3),raw(5*j+4)};c.input.offsets[j]={value(20+3*j),value(21+3*j),value(22+3*j)};}
            c.input.response_parameter=value(32);c.input.gain_parameter=value(33);c.input.scalar_divisor=value(34);c.input.bases={value(35),value(36),value(37)};
            if(raw(38)>1)throw rt::Fatal("invalid feedback flag");c.input.feedback_enabled=raw(38)!=0;
            for(unsigned j=0;j<3;++j){c.state.car.motion[j]=value(39+j);c.state.car.candidate_position[j]=value(42+j);c.state.car.candidate_orientation[j]=value(45+j);}
            c.state.car.feedback={std::bit_cast<std::int32_t>(raw(48)),std::bit_cast<std::int32_t>(raw(49))};c.state.contact_mask=raw(50);c.state.selected_scalar=value(51);
            cases::Words expected;for(unsigned j=0;j<expected.size();++j)expected[j]=raw(52+j);
            compare(program,initial,c,&expected);
        }
    }
    std::printf("CONTACT_PASS_RESPONSE_ORACLE PASS cases=%u live=%u cpu_instructions=%llu geometry_instructions=%llu hash=%016llx\n",count,live,(unsigned long long)instructions,(unsigned long long)geometry_instructions,(unsigned long long)fingerprint);
}catch(const std::exception& e){std::fprintf(stderr,"composition oracle failed: %s\n",e.what());return 1;}return 0;}
