// Offline comparison with the verified original per-contact CPU region.
#include "contact_response_cases.h"
#include "refcore/i960_ref.h"
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
namespace {
namespace cases=contact_response_cases;
constexpr std::uint32_t base=0x200000,node=base+0x100,frame=base+0x400,slots=0x2138d0;
struct Memory final:rt::Bus {
    const std::vector<std::uint8_t>& program;
    std::array<std::uint32_t,512> ram{};
    std::array<std::uint32_t,36> results{};
    std::uint32_t mask=0;
    Memory(const std::vector<std::uint8_t>& image):program(image){for(unsigned i=0;i<ram.size();++i)ram[i]=0xa5c71100u+i;for(unsigned i=0;i<results.size();++i)results[i]=0x51c90000u+i;}
    std::uint32_t& at(std::uint32_t a){
        if(a&3)throw rt::Fatal("unaligned response access");
        if(a==0x213960)return mask;
        if(a>=slots&&a<slots+144)return results[(a-slots)/4];
        if(a<base||a>=base+ram.size()*4)throw rt::Fatal("response access escaped bounded inputs");
        return ram[(a-base)/4];
    }
    std::uint32_t fetch(std::uint32_t a)override{
        if(a<0x5d3728||a>=0x5d3a30||a&3)throw rt::Fatal("response fetch escaped verified region");
        std::uint32_t word=0;for(unsigned i=0;i<4;++i)word|=std::uint32_t(program.at(a-0x59f000+i))<<(8*i);return word;
    }
    std::uint32_t read_dword(std::uint32_t a)override{return at(a);}
    std::uint8_t read_byte(std::uint32_t a)override{return std::uint8_t(at(a&~3u)>>(8*(a&3)));}
    std::uint16_t read_word(std::uint32_t)override{throw rt::Fatal("unexpected response halfword read");}
    void write_dword(std::uint32_t a,std::uint32_t value)override{at(a)=value;}
    void write_byte(std::uint32_t,std::uint8_t)override{throw rt::Fatal("unexpected response byte write");}
    void write_word(std::uint32_t,std::uint16_t)override{throw rt::Fatal("unexpected response halfword write");}
    std::uint16_t flags(std::uint32_t)override{return rt::Cpu::BURST;}
};
unsigned count=0,accepted=0,active=0,live=0;
std::uint64_t hash=14695981039346656037ull,instructions=0;
void compare(const std::vector<std::uint8_t>& program,const cases::Case& c,const cases::Words* captured=nullptr){
    auto native=cases::sentinel(),mathematical=cases::sentinel();
    const bool ok=ps2::native::make_contact_response(c.input,c.previous,native),expected=cases::mathematical(c,mathematical);
    if(ok!=expected||cases::words(native)!=cases::words(mathematical)||(!ok&&captured))throw rt::Fatal("response mathematical/native domain differs");
    if(ok){
        const unsigned index=count%4;const auto result_slot=slots+36*index,offset=frame+0x70+12*index,response=frame+0xa0+12*index;
        Memory memory(program);memory.mask=0xa5a5a5a5u;
        for(unsigned i=0;i<3;++i)memory.at(result_slot+12+4*i)=cases::bits(c.input.contact.direction[i]);
        memory.at(result_slot+24)=cases::bits(c.input.contact.gap);memory.at(result_slot+28)=c.input.contact.status;
        memory.at(offset)=cases::bits(c.input.offset.x);memory.at(offset+4)=cases::bits(c.input.offset.y);memory.at(offset+8)=cases::bits(c.input.offset.z);
        memory.at(node+0x50)=cases::bits(c.input.parameter50);memory.at(node+0x54)=cases::bits(c.input.parameter54);
        const auto before=memory.ram;const auto original_results=memory.results;
        rt::I960Ref cpu(&memory);for(auto& reg:cpu.m_r)reg=0xc5a9f713u;
        cpu.m_r[3]=result_slot+28;cpu.m_r[5]=index;cpu.m_r[6]=12*index;cpu.m_r[7]=frame+0xd0;
        cpu.m_r[9]=result_slot;cpu.m_r[10]=result_slot+24;cpu.m_r[12]=cases::bits(c.previous);cpu.m_r[13]=node;
        cpu.m_r[14]=frame+0xe0;cpu.m_r[15]=36*index;cpu.m_r[25]=frame+0xd8;cpu.m_r[30]=0;cpu.m_r[31]=frame;
        cpu.m_AC=0x3f001000;cpu.m_IP=0x5d3728;unsigned steps=0;
        while(cpu.m_IP!=0x5d3a30){if(++steps>220)throw rt::Fatal("response exceeded instruction bound");cpu.execute_one();}
        cases::Words original{memory.at(response),memory.at(response+4),memory.at(response+8),cpu.m_r[12],std::uint32_t(bool(c.input.contact.status&128))};
        if(original!=cases::words(native)||(captured&&original!=*captured)){
            for(unsigned i=0;i<5;++i)std::fprintf(stderr,"RESPONSE case=%u field=%u native=%08x original=%08x\n",count,i,cases::words(native)[i],original[i]);
            throw rt::Fatal("original/native/captured response differs");
        }
        for(unsigned i=0;i<before.size();++i){
            const auto address=base+4*i;
            if(address>=response&&address<response+12)continue;
            if(native.active&&((address>=frame+0xd0&&address<frame+0xec)||address==frame+0x110))continue;
            if(memory.ram[i]!=before[i])throw rt::Fatal("response changed preserved ordinary RAM");
        }
        if(memory.results!=original_results||memory.mask!=std::uint32_t(0xa5a5a5a5u+(native.active?(1u<<index):0u)))
            throw rt::Fatal("response changed interpreted reply or wrong mask bit");
        instructions+=steps;active+=native.active;
    }
    cases::hash(hash,cases::words(native),ok);++count;accepted+=ok;live+=captured!=nullptr;
}
std::vector<std::uint8_t> read(const char* path){std::ifstream f(path,std::ios::binary);if(!f)throw rt::Fatal("missing response input");return {std::istreambuf_iterator<char>(f),{}};}
std::uint32_t word(const std::vector<std::uint8_t>& b,std::size_t i){return std::uint32_t(b.at(i))|(std::uint32_t(b.at(i+1))<<8)|(std::uint32_t(b.at(i+2))<<16)|(std::uint32_t(b.at(i+3))<<24);}
}
int main(int argc,char** argv){
    if(argc!=2&&argc!=3)return 2;
    try{
        if(std::fegetround()!=FE_TONEAREST)throw rt::Fatal("response oracle requires nearest-even rounding");
        const auto program=read(argv[1]);if(program.size()!=2097152)throw rt::Fatal("wrong response program size");
        if(!cases::each([&](const auto& c){compare(program,c);return true;},true))return 1;
        if(count!=18432||accepted!=18360||hash!=0x3684bd3cf078fbc4ull)throw rt::Fatal("response synthetic coverage changed");
        if(argc==3){
            const auto records=read(argv[2]);constexpr std::array<std::uint8_t,8> magic{'R','E','S','P','O','N','S','1'};
            if(records.size()<12||!std::equal(magic.begin(),magic.end(),records.begin()))throw rt::Fatal("invalid response record magic");
            const auto n=word(records,8);if(!n||n>128||records.size()!=12ull+64ull*n)throw rt::Fatal("invalid response record count/length");
            for(unsigned i=0;i<n;++i){
                const auto raw=[&](unsigned j){return word(records,12+64*i+4*j);};
                cases::Case c{};for(unsigned j=0;j<3;++j)c.input.contact.direction[j]=cases::value(raw(j));
                c.input.offset={cases::value(raw(3)),cases::value(raw(4)),cases::value(raw(5))};
                c.input.contact.gap=cases::value(raw(6));c.previous=cases::value(raw(7));
                c.input.parameter50=cases::value(raw(8));c.input.parameter54=cases::value(raw(9));c.input.contact.status=raw(10);
                cases::Words captured;for(unsigned j=0;j<5;++j)captured[j]=raw(11+j);compare(program,c,&captured);
            }
        }
        std::printf("CONTACT_RESPONSE_ORACLE PASS cases=%u accepted=%u active=%u live=%u instructions=%llu hash=%016llx\n",count,accepted,active,live,(unsigned long long)instructions,(unsigned long long)hash);
    }catch(const std::exception& error){std::fprintf(stderr,"response oracle failed: %s\n",error.what());return 1;}
}
