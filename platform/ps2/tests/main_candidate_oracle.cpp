// Offline original CPU-region and geometry comparison. No original bytes embedded.
#include "main_candidate_cases.h"
#include "refcore/i960_ref.h"
#include "runtime/tgp.h"
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
namespace {
namespace cases=main_candidate_cases;
using Bytes=std::vector<std::uint8_t>;
Bytes read(const char* path){std::ifstream f(path,std::ios::binary);if(!f)throw rt::Fatal("missing private input");return {std::istreambuf_iterator<char>(f),{}};}
std::uint32_t word(const Bytes& b,std::size_t o){return b.at(o)|(std::uint32_t(b.at(o+1))<<8)|(std::uint32_t(b.at(o+2))<<16)|(std::uint32_t(b.at(o+3))<<24);}
struct Geometry final:rt::TgpBus {
    std::vector<std::uint32_t> input,output;unsigned consumed=0;
    bool fifo_pop(std::uint32_t& v)override{if(consumed==input.size())return false;v=input[consumed++];return true;}
    void fifo_push(std::uint32_t v)override{if(!output.empty())throw rt::TgpFatal("excess candidate metric output");output.push_back(v);}
    std::uint32_t mem_r(std::uint32_t)override{throw rt::TgpFatal("unexpected candidate metric external read");}
    void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("unexpected candidate metric external write");}
    void bank_w(std::uint32_t v)override{if(v)throw rt::TgpFatal("unexpected candidate metric bank");}
};
struct Done{};
void hook(rt::Tgp& t,std::uint16_t pc){
    if(!((pc>=0x51c&&pc<0x530)||(pc>=0x1ab&&pc<0x1cb))||t.count>52)throw rt::TgpFatal("candidate metric escaped verified region");
    if(pc==0x52f){if(t.pc!=0xfff)throw rt::TgpFatal("wrong candidate metric return");throw Done{};}
}
constexpr std::uint32_t base=0x210000,frame=base,node=base+0x1000,marker=0x213974;
struct Memory final:rt::Bus {
    const Bytes& image;rt::Tgp tgp;
    std::array<std::uint32_t,0x4000/4> ram{};std::vector<std::uint32_t> fifo,addresses;
    bool collecting=false;unsigned read_index=0,commands=0;std::uint64_t geometry_instructions=0;
    Memory(const Bytes& p,const rt::Tgp& initial):image(p),tgp(initial){for(unsigned i=0;i<ram.size();++i)ram[i]=0xa5310000u^i;}
    std::uint32_t& at(std::uint32_t a){if(a<base||a>=base+ram.size()*4||(a&3))throw rt::Fatal("candidate memory escaped bounded region");return ram[(a-base)/4];}
    bool writable(std::uint32_t a)const{return (a>=node+0x14&&a<=node+0x2c)||a==node+0x34||a==marker||
        (a>=frame+0x130&&a<=frame+0x138)||(a>=frame+0x140&&a<=frame+0x148);}
    std::uint32_t fetch(std::uint32_t a)override{
        if(a<0x5d3ff0||a>=0x5d4218||(a&3))throw rt::Fatal("candidate CPU fetch escaped region");return word(image,a-0x59f000);
    }
    std::uint32_t read_dword(std::uint32_t a)override{
        if(a==0x884000){if(collecting||read_index>=fifo.size())throw rt::Fatal("unexpected candidate metric read");return fifo[read_index++];}
        if(!((a>=node+0x14&&a<=node+0x4c)||a==frame+0x1f0))throw rt::Fatal("unexpected candidate memory read");return at(a);
    }
    void write_dword(std::uint32_t a,std::uint32_t v)override{
        if(a==0x884000){
            if(!collecting){if(read_index!=fifo.size()||v!=0x2c005858u)throw rt::Fatal("unexpected candidate geometry command");collecting=true;fifo.clear();read_index=0;return;}
            fifo.push_back(v);
            if(fifo.size()==6){
                Geometry bus;bus.input=fifo;tgp.bus=&bus;tgp.pc=0x51c;tgp.pcs[0]=0xfff;tgp.count=0;tgp.hook=hook;
                try{rt::tgpgen::run(tgp,53);throw rt::Fatal("candidate metric did not return");}catch(Done&){}
                if(bus.consumed!=6||bus.output.size()!=1||tgp.tables||tgp.bank_reg)throw rt::Fatal("candidate metric transfer counts differ");
                geometry_instructions+=tgp.count;++commands;fifo=bus.output;collecting=false;
            }
            return;
        }
        if(!writable(a))throw rt::Fatal("unexpected candidate memory write");addresses.push_back(a);at(a)=v;
    }
    std::uint8_t read_byte(std::uint32_t)override{throw rt::Fatal("unexpected candidate byte read");}
    std::uint16_t read_word(std::uint32_t)override{throw rt::Fatal("unexpected candidate halfword read");}
    void write_byte(std::uint32_t,std::uint8_t)override{throw rt::Fatal("unexpected candidate byte write");}
    void write_word(std::uint32_t,std::uint16_t)override{throw rt::Fatal("unexpected candidate halfword write");}
    std::uint16_t flags(std::uint32_t)override{return rt::Cpu::BURST;}
};
unsigned count=0,accepted=0,consumed=0,live=0;std::uint64_t hash=14695981039346656037ull,instructions=0,geometry_instructions=0;
void compare(const Bytes& program,const rt::Tgp& initial,const cases::MainCandidateState& before,const cases::Words* captured=nullptr){
    auto native=before;const bool ok=ps2::native::prepare_main_candidate(native);
    auto math=before;if(cases::mathematical(math)!=ok||cases::words(math)!=cases::words(native))throw rt::Fatal("candidate native/formula mismatch");
    const auto input=cases::words(before),output=cases::words(native);
    if(ok){
        Memory memory(program,initial);
        for(unsigned i=0;i<15;++i)memory.at(node+0x14+4*i)=input[i];
        memory.at(marker)=input[15];memory.at(frame+0x1f0)=node;
        for(unsigned i=0;i<3;++i){memory.at(frame+0x130+4*i)=input[16+i];memory.at(frame+0x140+4*i)=input[19+i];}
        auto expected=memory.ram;
        const auto set=[&](std::uint32_t a,std::uint32_t v){expected[(a-base)/4]=v;};
        for(unsigned i=0;i<15;++i)set(node+0x14+4*i,output[i]);set(marker,output[15]);
        for(unsigned i=0;i<3;++i){set(frame+0x130+4*i,output[16+i]);set(frame+0x140+4*i,output[19+i]);}
        rt::I960Ref cpu(&memory);cpu.m_AC=0x3f001000u;cpu.m_IP=0x5d3ff0;cpu.m_r[28]=node;cpu.m_r[31]=frame;
        unsigned steps=0;while(cpu.m_IP!=0x5d4218){if(++steps>140)throw rt::Fatal("candidate CPU instruction bound exceeded");cpu.execute_one();}
        if(memory.ram!=expected||(captured&&output!=*captured)||memory.commands!=1||memory.collecting||memory.read_index!=memory.fifo.size()||
           cpu.m_r[16]!=node||cpu.m_r[17]!=frame+0x130||cpu.m_r[18]!=frame+0x140){
            std::fprintf(stderr,"CANDIDATE MISMATCH case=%u\n",count);
            for(unsigned i=0;i<expected.size();++i)if(memory.ram[i]!=expected[i])std::fprintf(stderr,"address=%08x expected=%08x actual=%08x\n",base+4*i,expected[i],memory.ram[i]);
            throw rt::Fatal("candidate original/native/captured state mismatch");
        }
        instructions+=steps;geometry_instructions+=memory.geometry_instructions;++accepted;consumed+=native.response_marker==15;
    }else if(captured||input!=output)throw rt::Fatal("captured candidate rejected or failed atomic rejection");
    ++count;live+=captured!=nullptr;cases::hash(hash,input);cases::hash(hash,output);hash=(hash^ok)*1099511628211ull;
}
}
int main(int argc,char** argv){try{
    if(argc!=3&&argc!=4)return 2;if(std::fegetround()!=FE_TONEAREST)throw rt::Fatal("candidate oracle needs nearest-even mode");
    const auto program=read(argv[1]),firmware=read(argv[2]);if(program.size()!=2097152||firmware.size()!=6660)throw rt::Fatal("wrong private image size");
    rt::Tgp initial;Geometry startup;initial.reset();initial.bus=&startup;for(unsigned i=0;i<firmware.size()/4;++i)initial.prog[i]=word(firmware,4*i);
    startup.input={0x04000000};rt::tgpgen::run(initial,100000);initial.bus=nullptr;
    if(initial.count>=100000||startup.consumed!=1||startup.output!=std::vector<std::uint32_t>{8}||initial.tables||initial.bank_reg)throw rt::Fatal("candidate firmware startup failed");
    if(!cases::each([&](const auto& s){compare(program,initial,s);return true;},true))return 1;
    if(count!=17044||accepted!=16315||consumed!=5807||hash!=0x966c10f581152becull)throw rt::Fatal("candidate synthetic coverage changed");
    if(argc==4){
        const auto records=read(argv[3]);constexpr std::array<std::uint8_t,8> magic{'M','C','A','N','D','I','D','1'};
        if(records.size()<12||!std::equal(magic.begin(),magic.end(),records.begin()))throw rt::Fatal("invalid candidate records");
        const auto n=word(records,8);if(!n||n>64||records.size()!=12ull+176ull*n)throw rt::Fatal("invalid candidate record count");
        for(unsigned i=0;i<n;++i){cases::Words input,expected;for(unsigned j=0;j<22;++j){input[j]=word(records,12+176*i+4*j);expected[j]=word(records,12+176*i+88+4*j);}compare(program,initial,cases::state(input),&expected);}
    }
    std::printf("MAIN_CANDIDATE_ORACLE PASS cases=%u accepted=%u consumed=%u live=%u cpu_instructions=%llu geometry_instructions=%llu hash=%016llx\n",count,accepted,consumed,live,(unsigned long long)instructions,(unsigned long long)geometry_instructions,(unsigned long long)hash);
}catch(const std::exception& error){std::fprintf(stderr,"candidate oracle failed: %s\n",error.what());return 1;}return 0;}
