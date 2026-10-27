// Isolated original-program comparison. Not linked into native applications.
#include "main_balance_update_adapter.h"
#include "refcore/i960_ref.h"
#include "runtime/tgp.h"
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>

namespace {
namespace test=balance_update_test;
using Bytes=std::vector<std::uint8_t>;
Bytes read(const char* path){std::ifstream input(path,std::ios::binary);if(!input)throw rt::Fatal("missing balance oracle input");return {std::istreambuf_iterator<char>(input),{}};}
std::uint32_t word(const Bytes& v,std::size_t n){return v.at(n)|(std::uint32_t(v.at(n+1))<<8)|(std::uint32_t(v.at(n+2))<<16)|(std::uint32_t(v.at(n+3))<<24);}
struct Reader {const Bytes& bytes;std::size_t cursor=0;std::uint32_t get(){const auto x=word(bytes,cursor);cursor+=4;return x;}std::uint64_t wide(){const auto lo=get();return lo|(std::uint64_t(get())<<32);}};
struct Geometry:rt::TgpBus {
    std::vector<std::uint32_t> input,output;unsigned consumed=0;
    bool fifo_pop(std::uint32_t& v)override{if(consumed==input.size())return false;v=input[consumed++];return true;}
    void fifo_push(std::uint32_t v)override{if(output.size()>128)throw rt::TgpFatal("balance excessive geometry output");output.push_back(v);}
    std::uint32_t mem_r(std::uint32_t)override{throw rt::TgpFatal("balance external geometry read");}
    void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("balance external geometry write");}
    void bank_w(std::uint32_t v)override{if(v)throw rt::TgpFatal("balance geometry bank change");}
};
struct Memory:rt::Bus {
    const Bytes& image;rt::Tgp geometry;Geometry io;std::uint32_t stack=0;unsigned output_index=0;
    std::map<std::uint32_t,std::uint8_t> data;std::set<std::uint32_t> written;
    Memory(const Bytes& p,const rt::Tgp& initial):image(p),geometry(initial){geometry.bus=&io;}
    void set(std::uint32_t a,std::uint32_t value){for(unsigned i=0;i<4;++i)data[a+i]=value>>(8*i);}
    bool stack_address(std::uint32_t a)const{return a>=stack&&a<stack+4096;}
    bool queue(std::uint32_t a)const{return a>=0x20b100&&a<0x20b190;}
    std::uint8_t read_byte(std::uint32_t a)override{
        const auto found=data.find(a);if(found!=data.end())return found->second;
        if(a>=0x59f000&&a-0x59f000<image.size())return image[a-0x59f000];
        if(stack_address(a)||queue(a))return 0;
        std::fprintf(stderr,"balance unexpected read %08x\n",a);throw rt::Fatal("balance undeclared ordinary-memory input");
    }
    std::uint16_t read_word(std::uint32_t a)override{return read_byte(a)|(std::uint16_t(read_byte(a+1))<<8);}
    std::uint32_t read_dword(std::uint32_t a)override{
        if(a==0x884000){flush();if(output_index>=io.output.size())throw rt::Fatal("balance missing geometry result");return io.output[output_index++];}
        return read_word(a)|(std::uint32_t(read_word(a+2))<<16);
    }
    void write_byte(std::uint32_t a,std::uint8_t v)override{
        if(!data.contains(a)&&!stack_address(a)&&!queue(a)){std::fprintf(stderr,"balance unexpected write %08x\n",a);throw rt::Fatal("balance undeclared ordinary-memory output");}
        data[a]=v;written.insert(a);
    }
    void write_word(std::uint32_t a,std::uint16_t v)override{write_byte(a,v);write_byte(a+1,v>>8);}
    void write_dword(std::uint32_t a,std::uint32_t v)override{
        if(a==0x884000){if(io.input.size()>=256)throw rt::Fatal("balance excessive geometry command words");io.input.push_back(v);return;}
        write_word(a,v);write_word(a+2,v>>16);
    }
    void flush(){const auto before=geometry.count;rt::tgpgen::run(geometry,8192);if(geometry.count-before>=8192||io.consumed!=io.input.size()||geometry.pc!=0x77||geometry.tables||geometry.bank_reg||geometry.sincos_base||geometry.inv_base||geometry.isqrt_base)throw rt::Fatal("balance geometry completion/table boundary");}
    std::uint32_t fetch(std::uint32_t a)override{
        if(a<0x59f000)throw rt::Fatal("balance instruction relocation");
        const auto pc=a-0x59f000;
        constexpr std::array<std::array<std::uint32_t,2>,16> ranges{{{0x37870,0x38674},{0x38680,0x38ad0},{0x395d8,0x39668},
            {0x39750,0x39a90},{0x39b80,0x39f14},{0x3a320,0x3aa24},{0x3aa38,0x3ab88},{0x3ac50,0x3adf8},
            {0x3ae50,0x3af44},{0x3af58,0x3aff0},{0x3b008,0x3b118},{0x3b128,0x3b1ec},{0x5cce8,0x5cd28},
            {0x26090,0x260a4},{0x260cc,0x26150},{0x261b0,0x26200}}};
        for(const auto& range:ranges)if(pc>=range[0]&&pc<range[1]&&!(pc&3))return word(image,pc);
        std::fprintf(stderr,"balance unexpected instruction %08x\n",pc);throw rt::Fatal("balance unsupported original path");
    }
    std::uint16_t flags(std::uint32_t a)override{return a==0x884000?0:rt::Cpu::BURST;}
};
struct Case {
    std::array<std::uint32_t,32> registers{};std::array<std::uint64_t,4> floating{};
    std::uint32_t ac=0,frame=0;std::map<std::uint32_t,std::uint32_t> before,after;
};
Case case_read(Reader& source){
    Case c;c.frame=source.get();c.ac=source.get();for(auto& x:c.registers)x=source.get();for(auto& x:c.floating)x=source.wide();
    const auto count=source.get();if(count>32)throw rt::Fatal("balance excessive input blocks");
    for(unsigned i=0;i<count;++i){const auto address=source.get(),size=source.get();if(size>64||address&3)throw rt::Fatal("balance input block range");for(unsigned j=0;j<size;++j)c.before[address+4*j]=source.get();for(unsigned j=0;j<size;++j)c.after[address+4*j]=source.get();}
    return c;
}
std::uint64_t hash=14695981039346656037ull,cpu_steps=0,geometry_steps=0;unsigned total=0,commands=0,synthetic=0,rejected=0;
bool compare(const Bytes& program,const rt::Tgp& initial,const Case& c,bool captured=true){
    Memory memory(program,initial);memory.stack=c.registers[31];for(const auto&[a,v]:c.before)memory.set(a,v);
    test::OwnedAssets assets;const auto read=[&](std::uint32_t a){return memory.read_dword(a);};
    const auto curve=read(0x2142f8);if(curve!=0x5d8f20&&curve!=0x5d90c0)throw rt::Fatal("balance curve identity");
    test::assets(assets,read,curve);
    test::MainBalanceUpdateState state;
    const auto object=c.registers[16],first=c.registers[17],second=c.registers[18];
    test::state_words(state,object,first,second,[&](std::uint32_t a,auto& value){using T=std::remove_reference_t<decltype(value)>;if constexpr(sizeof(T)==1)value=memory.read_byte(a);else value=std::bit_cast<T>(read(a));});
    auto input=test::inputs(read,c.registers[19]);test::MainBalanceEffects effects;
    if(!test::update_main_balance(assets.values,input,state,effects)){
        if(!captured){++rejected;return false;}
        std::fprintf(stderr,"balance native rejected frame=%u case=%u\n",c.frame,total);throw rt::Fatal("balance native rejected actual input");
    }
    auto expected=captured?c.after:c.before;
    if(!captured)test::state_words(state,object,first,second,[&](std::uint32_t a,const auto& value){
        if constexpr(sizeof(value)==1){auto& word=expected.at(a&~3u);const auto shift=(a&3)*8;word=(word&~(255u<<shift))|(test::bits(value)<<shift);}
        else expected.at(a)=test::bits(value);
    });
    rt::I960Ref cpu(&memory);std::copy(c.registers.begin(),c.registers.end(),cpu.m_r);for(unsigned i=0;i<4;++i)cpu.m_fp[i]=std::bit_cast<double>(c.floating[i]);
    cpu.m_AC=c.ac;cpu.m_IP=0x5d6870;std::vector<test::MainBalanceCommand> emitted;
    const auto before=memory.geometry.count;unsigned steps=0;
    while(cpu.m_IP!=0x5d7670){
        if(++steps>6000)throw rt::Fatal("balance instruction bound");
        if(cpu.m_IP==0x5c5140||cpu.m_IP==0x5c51b0)emitted.push_back({cpu.m_r[16],cpu.m_IP==0x5c51b0?cpu.m_r[17]:0,cpu.m_IP==0x5c51b0});
        cpu.execute_one();
    }
    memory.flush();
    unsigned failures=0;
    test::state_words(state,object,first,second,[&](std::uint32_t a,const auto& value){
        const auto native=test::bits(value);const auto original=sizeof(value)==1?memory.read_byte(a):read(a);
        const auto found=expected.find(a&~3u);if(found==expected.end())throw rt::Fatal("balance expected output missing");
        const auto captured=sizeof(value)==1?(found->second>>((a&3u)*8))&255:found->second;
        if(native!=original||native!=captured){std::fprintf(stderr,"BALANCE frame=%u address=%08x native=%08x original=%08x captured=%08x\n",c.frame,a,native,original,captured);++failures;}
        for(auto w:{a,native}){hash^=w;hash*=1099511628211ull;}
    });
    if(failures)throw rt::Fatal("balance native/original/capture mismatch");
    if(emitted.size()!=effects.count)throw rt::Fatal("balance emitted command count differs");
    for(unsigned i=0;i<effects.count;++i){const auto& a=emitted[i];const auto& b=effects.commands[i];if(a.code!=b.code||a.parameter!=b.parameter||a.has_parameter!=b.has_parameter)throw rt::Fatal("balance emitted command differs");}
    for(const auto&[a,v]:expected){if(memory.stack_address(a))continue;if(read(a)!=v){std::fprintf(stderr,"BALANCE frame=%u unmapped word=%08x original=%08x captured=%08x\n",c.frame,a,read(a),v);throw rt::Fatal("balance original whole captured memory mismatch");}}
    if(memory.geometry.data[7]!=initial.data[7]||memory.geometry.data[9]!=initial.data[9]||memory.output_index!=memory.io.output.size())throw rt::Fatal("balance geometry state boundary");
    ++total;synthetic+=!captured;cpu_steps+=steps;geometry_steps+=memory.geometry.count-before;commands+=effects.count;return true;
}
void synthetic_cases(const Bytes& program,const rt::Tgp& initial,const Case& seed){
    // Originally authored boundary/control variations over a private captured
    // state. No original values are embedded in the source or public fixture.
    const auto object=seed.registers[16];
    for(unsigned variation=0;variation<512;++variation){
        Case sample=seed;sample.frame=0x10000+variation;
        auto& data=sample.before;
        data[0x202008]=variation;data[0x214120]=variation%3;
        data[0x2020b4]=(variation>>3)&1;data[0x2139d0]=(variation>>4)&1;
        data[0x2139cc]=(variation>>5)&1;data[0x202044]=(variation/3)%5;
        data[object+0x78]=(variation/7)%5;data[0x2142e0]=31;
        data[0x2142fc]=(variation>>6)&1?6:0;data[0x214300]=(variation>>7)&1?std::uint32_t(-8):0;
        data[0x2142c8]=(variation>>2)%5;data[0x2142cc]=variation%3;
        data[0x2142d0]=std::uint32_t(int(variation%65)-32);
        data[0x214330]=variation%3;data[0x21432c]=variation%3?61:0;
        data[0x214250]=variation%3?11:59;data[0x214208]=1u<<(variation%10);
        const unsigned throttle=std::array<unsigned,5>{0,48,128,191,255}[variation%5];
        const unsigned brake=std::array<unsigned,5>{0,48,64,128,255}[(variation/5)%5];
        data[0x202050]=(data[0x202050]&0xff000000u)|(variation&255)|(throttle<<8)|(brake<<16);
        data[0x202018]=(data[0x202018]&0xff00ffffu)|(((variation>>2)&1)<<16);
        data[0x202098]=(variation>>3)%5;
        data[object+0x7c]=std::array<std::uint32_t,4>{0x447a0000,0x44fa0000,0x45bb8000,0x46098000}[variation%4];
        data[0x2142e8]=data[object+0x7c];
        data[0x20b0b4]=std::array<std::uint32_t,4>{0,0x3c23d70a,0x3dcccccd,0x3e800000}[(variation/4)%4];
        data[object+0x3c]=std::array<std::uint32_t,4>{0,0x3e800000,0xbe800000,0x3f800000}[(variation/16)%4];
        data[object+0x74]=(variation&1)?0x3e800000u:0xbe800000u;
        data[object+0x2c]=0;data[object+0x30]=0;data[object+0x34]=(variation&1)?0x3f800000u:0xbf800000u;
        if(variation%7==0)for(unsigned i=0;i<4;++i)data[object+0x64+4*i]=0;
        (void)compare(program,initial,sample,false);
    }
}
}
int main(int argc,char** argv){try{
    if(argc!=4||std::fegetround()!=FE_TONEAREST)return 2;
    const auto program=read(argv[1]),firmware=read(argv[2]),records=read(argv[3]);
    if(program.size()!=2097152||firmware.size()!=6660)throw rt::Fatal("balance private image size");
    rt::Tgp initial;Geometry startup;initial.reset();initial.bus=&startup;for(unsigned i=0;i<firmware.size()/4;++i)initial.prog[i]=word(firmware,4*i);
    startup.input={0x04000000};rt::tgpgen::run(initial,1000);
    if(initial.count!=108||initial.pc!=0x77||startup.output!=std::vector<std::uint32_t>{8}||initial.tables)throw rt::Fatal("balance firmware startup");
    Reader reader{records};if(reader.wide()!=0x313054445055424dull)throw rt::Fatal("balance record format");
    const auto count=reader.get();if(!count||count>1201)throw rt::Fatal("balance record count");
    Case seed;for(unsigned i=0;i<count;++i){auto current=case_read(reader);if(i==count/2)seed=current;(void)compare(program,initial,current);}
    if(reader.cursor!=records.size())throw rt::Fatal("balance trailing records");
    synthetic_cases(program,initial,seed);
    std::printf("MAIN_BALANCE_UPDATE_ORACLE PASS live=%u synthetic=%u unsupported_synthetic=%u commands=%u cpu_instructions=%llu geometry_instructions=%llu hash=%016llx\n",count,synthetic,rejected,commands,(unsigned long long)cpu_steps,(unsigned long long)geometry_steps,(unsigned long long)hash);
}catch(const std::exception& error){std::fprintf(stderr,"main balance update oracle failed: %s\n",error.what());return 1;}return 0;}
