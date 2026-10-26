// Offline original-program comparison; excluded from the native application.
#include "paired_balance_cases.h"
#include "vehicle_parameter_selection.h"
#include "refcore/i960_ref.h"
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
namespace cases=paired_balance_cases;
using namespace ps2::native;
constexpr std::uint32_t node=0x220000,entry=0x5d6900,end=0x5d69d0;
std::vector<std::uint8_t> read(const char* path){std::ifstream f(path,std::ios::binary);if(!f)throw rt::Fatal("missing private comparison input");return {std::istreambuf_iterator<char>(f),{}};}
std::uint32_t word(const std::vector<std::uint8_t>& data,std::size_t offset){return data.at(offset)|(std::uint32_t(data.at(offset+1))<<8)|(std::uint32_t(data.at(offset+2))<<16)|(std::uint32_t(data.at(offset+3))<<24);}
struct Memory final:rt::Bus {
    const std::vector<std::uint8_t>& program;cases::Words input{};std::vector<std::uint32_t> reads;
    explicit Memory(const std::vector<std::uint8_t>& p):program(p){}
    std::uint32_t fetch(std::uint32_t address)override{if(address<entry||address>=end||(address&3))throw rt::Fatal("balance fetch escaped region");return word(program,address-0x59f000);}
    std::uint32_t read_dword(std::uint32_t address)override{
        reads.push_back(address);if(address==0x21425c)return 0xa5aa5a55u;
        if(address>=node+0x64&&address<=node+0x70&&!(address&3))return input[(address-node-0x64)/4];
        throw rt::Fatal("balance read escaped explicit fields");
    }
    void write_dword(std::uint32_t,std::uint32_t)override{throw rt::Fatal("unexpected balance memory write");}
    std::uint8_t read_byte(std::uint32_t)override{throw rt::Fatal("unexpected byte read");}
    std::uint16_t read_word(std::uint32_t)override{throw rt::Fatal("unexpected halfword read");}
    void write_byte(std::uint32_t,std::uint8_t)override{throw rt::Fatal("unexpected byte write");}
    void write_word(std::uint32_t,std::uint16_t)override{throw rt::Fatal("unexpected halfword write");}
    std::uint16_t flags(std::uint32_t)override{return rt::Cpu::BURST;}
};
unsigned count=0,live_count=0;std::uint64_t instructions=0,fingerprint=14695981039346656037ull;
void compare(const std::vector<std::uint8_t>& image,const cases::Input& input,const cases::Words* captured=nullptr){
    PairedBalance native;if(!make_paired_balance(input,native))throw rt::Fatal("native balance rejected comparison input");
    if(cases::words(native)!=cases::words(cases::mathematical(input)))throw rt::Fatal("native balance differs from independent mathematical reference");
    Memory memory(image);for(unsigned i=0;i<4;++i)memory.input[i]=cases::bits(input[i]);const auto before=memory.input;
    rt::I960Ref cpu(&memory);cpu.m_AC=0x3f001000;cpu.m_IP=entry;cpu.m_r[9]=node;
    unsigned steps=0;while(cpu.m_IP!=end){if(++steps>56)throw rt::Fatal("balance instruction bound exceeded");cpu.execute_one();}
    const cases::Words actual{cpu.m_r[5],cpu.m_r[7],cpu.m_r[14],cpu.m_r[13]};
    std::vector<std::uint32_t> expected{node+0x64,node+0x68,node+0x6c,node+0x70,0x21425c};
    if(actual[0]&0x7fffffffu){expected.push_back(node+0x68);expected.push_back(node+0x64);}
    if(actual[1]&0x7fffffffu){expected.push_back(node+0x70);expected.push_back(node+0x6c);}
    if(actual!=cases::words(native)||(captured&&actual!=*captured)||memory.input!=before||memory.reads!=expected||cpu.m_r[12]!=0xa5aa5a55u||(cpu.m_AC>>30))
        throw rt::Fatal("original balance output/read/state differs");
    ++count;live_count+=captured!=nullptr;instructions+=steps;cases::hash(fingerprint,actual);
}

constexpr std::uint32_t selection_entry=0x5d689c,selection_end=0x5d68f4,table=0x5d67a0,destination=0x2141c0;
struct SelectionMemory final:rt::Bus {
    const std::vector<std::uint8_t>& program;
    std::array<VehicleParameterRecord,10> records{};std::array<std::uint32_t,10> aliases{};
    std::array<std::uint32_t,13> output{};std::uint32_t flags_value=0;
    std::vector<std::uint32_t> reads,writes;
    explicit SelectionMemory(const std::vector<std::uint8_t>& p):program(p){}
    std::uint32_t fetch(std::uint32_t a)override{if(a<selection_entry||a>=selection_end||(a&3))throw rt::Fatal("selection fetch escaped region");return word(program,a-0x59f000);}
    std::uint32_t read_dword(std::uint32_t a)override{
        reads.push_back(a);if(a==0x214208)return flags_value;
        if(a>=table&&a<table+40&&!(a&3))return node+0x100+64*aliases[(a-table)/4];
        if(a>=node+0x100&&a<node+0x100+640&&!(a&3)){const auto offset=a-node-0x100;const unsigned index=offset/64,component=(offset%64)/4;if(component<13)return records[index].words[component];}
        throw rt::Fatal("selection read escaped typed records");
    }
    void write_dword(std::uint32_t a,std::uint32_t v)override{
        if(a<destination||a>=destination+52||(a&3))throw rt::Fatal("selection write escaped output");
        writes.push_back(a);output[(a-destination)/4]=v;
    }
    std::uint8_t read_byte(std::uint32_t)override{throw rt::Fatal("unexpected selection byte read");}
    std::uint16_t read_word(std::uint32_t)override{throw rt::Fatal("unexpected selection halfword read");}
    void write_byte(std::uint32_t,std::uint8_t)override{throw rt::Fatal("unexpected selection byte write");}
    void write_word(std::uint32_t,std::uint16_t)override{throw rt::Fatal("unexpected selection halfword write");}
    std::uint16_t flags(std::uint32_t)override{return rt::Cpu::BURST;}
};
unsigned selection_count=0,selection_live=0;std::uint64_t selection_instructions=0,selection_fingerprint=14695981039346656037ull;
void selection(const std::vector<std::uint8_t>& image,std::uint32_t flags,const std::array<VehicleParameterRecord,10>& records,
               const std::array<std::uint32_t,10>& aliases,const VehicleParameterSelection* captured=nullptr){
    SelectionMemory memory(image);memory.flags_value=flags;memory.records=records;memory.aliases=aliases;
    auto expanded=records;for(unsigned i=0;i<10;++i)expanded[i]=records[aliases[i]];
    const auto native=select_vehicle_parameters(flags,expanded);
    rt::I960Ref cpu(&memory);cpu.m_IP=selection_entry;cpu.m_AC=0x3f001000;
    unsigned steps=0;while(cpu.m_IP!=selection_end){if(++steps>43)throw rt::Fatal("selection instruction bound exceeded");cpu.execute_one();}
    std::vector<std::uint32_t> reads{0x214208,table+4*native.index},writes;
    for(unsigned i=0;i<13;++i){reads.push_back(node+0x100+64*aliases[native.index]+4*i);writes.push_back(destination+4*i);}
    for(unsigned i=0;i<10;++i)if(memory.records[i].words!=records[i].words)throw rt::Fatal("selection modified input record");
    if(memory.output!=native.record.words||cpu.m_r[10]!=native.index||memory.reads!=reads||memory.writes!=writes||
       (captured&&(captured->index!=native.index||captured->record.words!=native.record.words)))throw rt::Fatal("original parameter selection differs");
    for(auto v:native.record.words)selection_fingerprint=(selection_fingerprint^v)*1099511628211ull;
    selection_fingerprint=(selection_fingerprint^native.index)*1099511628211ull;
    ++selection_count;selection_live+=captured!=nullptr;selection_instructions+=steps;
}
} // namespace
int main(int argc,char**argv){try{
    if(argc<2||argc>4)return 2;if(std::fegetround()!=FE_TONEAREST)throw rt::Fatal("nearest-even host rounding required");
    const auto image=read(argv[1]);if(image.size()!=2097152)throw rt::Fatal("wrong private program size");
    cases::each([&](const auto& input){compare(image,input);return true;},true);
    if(count!=101130||fingerprint!=0x205c18818a38f128ull)throw rt::Fatal("balance synthetic coverage changed");
    std::array<VehicleParameterRecord,10> records{};std::array<std::uint32_t,10> aliases{};std::uint32_t state=0x1976ad25u;
    for(auto& record:records)for(auto& v:record.words)v=cases::next(state);
    for(unsigned mode=0;mode<2;++mode){for(unsigned i=0;i<10;++i)aliases[i]=mode?i/2:i;
        for(std::uint32_t flags=0;flags<1024;++flags)for(auto high:{0u,0xfffffc00u,0xaaaaa800u})selection(image,flags|high,records,aliases);
    }
    if(argc>=3){const auto data=read(argv[2]);const std::array<std::uint8_t,8> magic{'P','A','I','R','B','A','L','1'};
        if(data.size()<12||!std::equal(magic.begin(),magic.end(),data.begin()))throw rt::Fatal("invalid pair capture record");
        const auto n=word(data,8);if(!n||n>64||data.size()!=12ull+32ull*n)throw rt::Fatal("invalid pair capture size");
        for(unsigned i=0;i<n;++i){cases::Input input;cases::Words expected;for(unsigned j=0;j<4;++j){input[j]=cases::value(word(data,12+32*i+4*j));expected[j]=word(data,28+32*i+4*j);}compare(image,input,&expected);}
    }
    if(argc==4){const auto data=read(argv[3]);const std::array<std::uint8_t,8> magic{'V','P','A','R','A','M','S','1'};
        if(data.size()<12||!std::equal(magic.begin(),magic.end(),data.begin()))throw rt::Fatal("invalid parameter capture record");
        const auto n=word(data,8);if(!n||n>64||data.size()!=12ull+60ull*n)throw rt::Fatal("invalid parameter capture size");
        for(unsigned i=0;i<10;++i){aliases[i]=i;const auto source=word(image,0x377a0+4*i);if(source<0x59f000||source-0x59f000>image.size()-52||(source&3))throw rt::Fatal("private parameter source outside verified image");for(unsigned j=0;j<13;++j)records[i].words[j]=word(image,source-0x59f000+4*j);}
        for(unsigned i=0;i<n;++i){VehicleParameterSelection expected;expected.index=word(data,16+60*i);for(unsigned j=0;j<13;++j)expected.record.words[j]=word(data,20+60*i+4*j);selection(image,word(data,12+60*i),records,aliases,&expected);}
    }
    std::printf("PAIRED_BALANCE_ORACLE PASS cases=%u live=%u instructions=%llu hash=%016llx\n",count,live_count,(unsigned long long)instructions,(unsigned long long)fingerprint);
    std::printf("VEHICLE_PARAMETER_ORACLE PASS cases=%u live=%u instructions=%llu hash=%016llx\n",selection_count,selection_live,(unsigned long long)selection_instructions,(unsigned long long)selection_fingerprint);
}catch(const std::exception& e){std::fprintf(stderr,"paired balance oracle failed: %s\n",e.what());return 1;}return 0;}
