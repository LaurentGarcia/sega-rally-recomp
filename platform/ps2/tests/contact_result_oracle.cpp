// Offline original CPU/geometry comparison. No original images are embedded.
#include "contact_result_cases.h"
#include "contact_search_cases.h"
#include "refcore/i960_ref.h"
#include "runtime/tgp.h"
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
namespace cases=contact_result_cases;
std::vector<std::uint8_t> read(const char* path){std::ifstream s(path,std::ios::binary);if(!s)throw rt::Fatal("cannot open private adapter input");return {std::istreambuf_iterator<char>(s),{}};}
std::uint32_t word(const std::vector<std::uint8_t>& b,std::size_t i){return b.at(i)|(std::uint32_t(b.at(i+1))<<8)|(std::uint32_t(b.at(i+2))<<16)|(std::uint32_t(b.at(i+3))<<24);}
struct NormalBus final:rt::TgpBus {
    std::array<std::uint32_t,3> input{},output{};unsigned consumed=0,produced=0;
    bool fifo_pop(std::uint32_t& v)override{if(consumed==3)throw rt::TgpFatal("normal excess input");v=input[consumed++];return true;}
    void fifo_push(std::uint32_t v)override{if(produced==3)throw rt::TgpFatal("normal excess output");output[produced++]=v;}
    std::uint32_t mem_r(std::uint32_t)override{throw rt::TgpFatal("normal external read");}
    void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("normal external write");}
};
struct Finished{};
void normal_hook(rt::Tgp& t,std::uint16_t pc){
    if(pc!=0x30d+t.count||t.count>25)throw rt::TgpFatal("normal escaped verified sequence");
    if(pc==0x326){if(t.pc!=0xfff)throw rt::TgpFatal("normal wrong return");throw Finished{};}
}
constexpr std::uint32_t base=0x200000,point_address=base+0x100,output_address=base+0x200,frame=base+0x400;
struct Memory final:rt::Bus {
    const std::vector<std::uint8_t>& program;
    const std::vector<std::uint8_t>& firmware;
    const std::array<std::uint32_t,65536>& tables;
    std::array<std::uint32_t,512> ram{};
    std::array<std::uint32_t,17> reply;
    std::vector<std::uint32_t> fifo,command;
    unsigned cursor=0,reads=0,writes=0,normal_calls=0;
    Memory(const std::vector<std::uint8_t>& p,const std::vector<std::uint8_t>& f,const std::array<std::uint32_t,65536>& t,
           const ps2::native::ContactPolygon& polygon):program(p),firmware(f),tables(t),reply(contact_search_cases::payload(polygon)){
        for(unsigned i=0;i<ram.size();++i)ram[i]=0x73a19b00u^i;
    }
    std::uint32_t fetch(std::uint32_t address)override{
        if(address<0x5c9cc0||address>=0x5c9ec4||(address&3))throw rt::Fatal("adapter fetch outside verified span");
        return word(program,address-0x59f000);
    }
    std::uint32_t& at(std::uint32_t address){
        if(address<base||address>=base+ram.size()*4||(address&3))throw rt::Fatal("adapter RAM outside bounds");
        return ram.at((address-base)/4);
    }
    std::uint32_t read_dword(std::uint32_t address)override{
        if(address!=0x884000)return at(address);
        if(cursor>=fifo.size())throw rt::Fatal("adapter FIFO underflow");++reads;return fifo[cursor++];
    }
    void write_dword(std::uint32_t address,std::uint32_t value)override{
        if(address!=0x884000){at(address)=value;return;}
        ++writes;command.push_back(value);
        if(command.front()==0x29805353u){
            if(command.size()==2){if(command[1]>14||(command[1]&1))throw rt::Fatal("unsupported adapter slot");fifo.insert(fifo.end(),reply.begin(),reply.end());command.clear();}
        }else if(command.front()==0x17802f2fu){
            if(command.size()==4){
                NormalBus bus;for(unsigned i=0;i<3;++i)bus.input[i]=command[i+1];
                rt::Tgp t;t.reset();t.bus=&bus;t.tables=tables.data();t.pc=0x30e;t.pcs[0]=0xfff;t.hook=normal_hook;
                for(unsigned i=0;i<firmware.size()/4;++i)t.prog[i]=word(firmware,4*i);
                try{rt::tgpgen::run(t,26);throw rt::TgpFatal("normal did not return");}catch(const Finished&){}
                if(t.count!=25||bus.consumed!=3||bus.produced!=3)throw rt::TgpFatal("normal incomplete transport");
                fifo.insert(fifo.end(),bus.output.begin(),bus.output.end());command.clear();++normal_calls;
            }
        }else throw rt::Fatal("unexpected adapter command");
    }
    std::uint8_t read_byte(std::uint32_t)override{throw rt::Fatal("unexpected adapter byte read");}
    std::uint16_t read_word(std::uint32_t)override{throw rt::Fatal("unexpected adapter halfword read");}
    void write_byte(std::uint32_t,std::uint8_t)override{throw rt::Fatal("unexpected adapter byte write");}
    void write_word(std::uint32_t,std::uint16_t)override{throw rt::Fatal("unexpected adapter halfword write");}
    std::uint16_t flags(std::uint32_t)override{return rt::Cpu::BURST;}
};
unsigned comparisons=0,planes=0,edges=0,misses=0,live=0;
std::uint64_t fingerprint=14695981039346656037ull,cpu_steps=0;
void compare(const cases::Case& c,const std::vector<std::uint8_t>& program,const std::vector<std::uint8_t>& firmware,
             const std::array<std::uint32_t,65536>& tables,std::span<const ps2::native::ContactNormalPair> pairs,
             const cases::Words* captured=nullptr){
    auto native=c.previous;
    if(!ps2::native::interpret_contact_result(c.polygon,c.point,pairs,native))throw rt::Fatal("supported adapter case rejected");
    if(cases::words(native)!=cases::words(cases::mathematical(c,pairs)))throw rt::Fatal("adapter differs from mathematical contract");
    Memory memory(program,firmware,tables,c.polygon);
    memory.at(point_address)=std::bit_cast<std::uint32_t>(c.point.x);
    memory.at(point_address+4)=std::bit_cast<std::uint32_t>(c.point.y);
    memory.at(point_address+8)=std::bit_cast<std::uint32_t>(c.point.z);
    const auto initial=cases::words(c.previous);
    for(unsigned i=0;i<5;++i)memory.at(output_address+12+4*i)=initial[i];
    const auto before=memory.ram;
    rt::I960Ref original(&memory);original.m_IP=0x5c9cc0;original.m_AC=0x3f001000;
    original.m_r[1]=frame+0x40;original.m_r[31]=frame;
    original.m_r[16]=point_address;original.m_r[17]=output_address;original.m_r[18]=comparisons%8;
    unsigned steps=0;
    while(original.m_IP!=0x5c9e68&&original.m_IP!=0x5c9eb4&&original.m_IP!=0x5c9ec0){
        if(++steps>110)throw rt::Fatal("adapter exceeded instruction bound");original.execute_one();
    }
    cases::Words expected;for(unsigned i=0;i<5;++i)expected[i]=memory.at(output_address+12+4*i);
    if(expected!=cases::words(native)||(captured&&expected!=*captured)){
        const auto actual=cases::words(native);
        for(unsigned i=0;i<5;++i)std::fprintf(stderr,"ADAPTER case=%u field=%u original=%08x native=%08x captured=%08x\n",comparisons,i,expected[i],actual[i],captured?(*captured)[i]:0);
        throw rt::Fatal("native/original/captured adapter result differs");
    }
    const bool plane=c.polygon.attributes&&!(c.polygon.attributes&0x800000u);
    const auto expected_return=!c.polygon.attributes?0x5c9ec0u:(plane?0x5c9e68u:0x5c9eb4u);
    if(original.m_IP!=expected_return)throw rt::Fatal("adapter used an unexpected return branch");
    if(memory.normal_calls!=unsigned(plane)||memory.reads!=(plane?20u:17u)||memory.writes!=(plane?6u:2u)||
       memory.cursor!=memory.fifo.size()||!memory.command.empty())throw rt::Fatal("adapter unexpected command transport");
    for(unsigned i=0;i<before.size();++i){
        const auto address=base+i*4;
        if(address>=frame+0x40&&address<frame+0x90)continue;
        if(address>=output_address+12&&address<output_address+32)continue;
        if(memory.ram[i]!=before[i])throw rt::Fatal("adapter changed point/prefix/guard storage");
    }
    if(plane)++planes;else if(c.polygon.attributes)++edges;else ++misses;
    cpu_steps+=steps;cases::hash(fingerprint,expected);++comparisons;if(captured)++live;
}
} // namespace
int main(int argc,char** argv){
    if(argc!=4&&argc!=5)return 2;
    try{
        if(std::fegetround()!=FE_TONEAREST)throw rt::Fatal("oracle requires nearest-even rounding");
        const auto program=read(argv[1]),firmware=read(argv[2]),raw_tables=read(argv[3]);
        if(program.size()!=2097152||firmware.size()!=6660||raw_tables.size()!=262144)throw rt::Fatal("wrong adapter input size");
        std::array<std::uint32_t,65536> tables;
        std::array<ps2::native::ContactNormalPair,8192> pairs;
        for(unsigned i=0;i<tables.size();++i)tables[i]=word(raw_tables,4*i);
        for(unsigned i=0;i<pairs.size();++i)pairs[i]={cases::value(tables[0xc000+2*i]&0x7fffffffu),cases::value(tables[0xc001+2*i])};
        if(!cases::each([&](const auto& c){compare(c,program,firmware,tables,pairs);return true;},true))return 1;
        if(comparisons!=40392||planes!=13529||edges!=13529||misses!=13334||
           cpu_steps!=2494675||fingerprint!=0x511421beac689f89ull)
            throw rt::Fatal("synthetic adapter coverage/fingerprint changed");
        if(argc==5){
            const auto records=read(argv[4]);
            if(records.empty()||records.size()%120||records.size()>4096*120)throw rt::Fatal("invalid live adapter records");
            for(std::size_t offset=0;offset<records.size();offset+=120){
                cases::Case c{};c.point={cases::value(word(records,offset)),cases::value(word(records,offset+4)),cases::value(word(records,offset+8))};
                for(unsigned i=0;i<4;++i){
                    c.polygon.vertices[i]={cases::value(word(records,offset+12+12*i)),cases::value(word(records,offset+16+12*i)),cases::value(word(records,offset+20+12*i))};
                    c.polygon.coefficients[i]=cases::value(word(records,offset+60+4*i));
                }
                c.polygon.attributes=word(records,offset+76);
                cases::Words expected;
                for(unsigned i=0;i<3;++i)c.previous.direction[i]=cases::value(word(records,offset+80+4*i));
                c.previous.gap=cases::value(word(records,offset+92));c.previous.status=word(records,offset+96);
                for(unsigned i=0;i<5;++i)expected[i]=word(records,offset+100+4*i);
                compare(c,program,firmware,tables,pairs,&expected);
            }
        }
        std::printf("CONTACT_RESULT_ORACLE PASS cases=%u live=%u planes=%u edges=%u misses=%u cpu_instructions=%llu normal_instructions=%u hash=%016llx\n",comparisons,live,planes,edges,misses,(unsigned long long)cpu_steps,planes*25,(unsigned long long)fingerprint);
    }catch(const std::exception& e){std::fprintf(stderr,"contact result oracle failed: %s\n",e.what());return 1;}
}
