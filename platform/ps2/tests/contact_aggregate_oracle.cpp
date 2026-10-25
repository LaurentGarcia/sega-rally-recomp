// Offline comparison against verified original instruction regions only.
#include "contact_aggregate_cases.h"
#include "refcore/i960_ref.h"
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
namespace {
namespace cases=contact_aggregate_cases;
constexpr std::uint32_t base=0x200000,node=base+0x100,frame=base+0x400,position=base+0x700,orientation=base+0x720;
struct Memory final:rt::Bus {
    const std::vector<std::uint8_t>& image;
    std::array<std::uint32_t,512> ram{};
    std::array<std::uint32_t,2> feedback{};
    std::uint32_t enable=0;
    std::vector<std::uint32_t> reads,writes;
    Memory(const std::vector<std::uint8_t>& source):image(source){for(unsigned i=0;i<ram.size();++i)ram[i]=0xa5b63100u+i;}
    std::uint32_t& at(std::uint32_t address){
        if(address==0x2142c8)return enable;
        if(address==0x213964||address==0x213968)return feedback[(address-0x213964)/4];
        if(address&3||address<base||address>=base+ram.size()*4)throw rt::Fatal("aggregate data escaped bounded state");
        return ram[(address-base)/4];
    }
    std::uint32_t fetch(std::uint32_t address)override{
        if(address&3||!((address>=0x5d3b5c&&address<0x5d3b84)||(address>=0x5d3b90&&address<0x5d3cc0)||address==0x5d3b48))
            throw rt::Fatal("aggregate fetch escaped verified regions");
        const auto offset=address-0x59f000;std::uint32_t word=0;
        for(unsigned i=0;i<4;++i)word|=std::uint32_t(image.at(offset+i))<<(8*i);return word;
    }
    std::uint32_t read_dword(std::uint32_t address)override{reads.push_back(address);return at(address);}
    void write_dword(std::uint32_t address,std::uint32_t value)override{writes.push_back(address);at(address)=value;}
    std::uint8_t read_byte(std::uint32_t)override{throw rt::Fatal("unexpected byte access");}
    std::uint16_t read_word(std::uint32_t)override{throw rt::Fatal("unexpected halfword access");}
    void write_byte(std::uint32_t,std::uint8_t)override{throw rt::Fatal("unexpected byte access");}
    void write_word(std::uint32_t,std::uint16_t)override{throw rt::Fatal("unexpected halfword access");}
    std::uint16_t flags(std::uint32_t)override{return rt::Cpu::BURST;}
};
std::uint64_t fingerprint=14695981039346656037ull,instructions=0;
unsigned steps=0,updates=0,live_steps=0,live_updates=0;
void step(const std::vector<std::uint8_t>& image,const cases::Step& c,const std::uint32_t* captured=nullptr){
    float native=c.accumulator;
    if(!ps2::native::accumulate_contact_scalar(c.accumulator,c.metric,c.sign,native)||cases::bits(native)!=cases::bits(cases::step_math(c)))
        throw rt::Fatal("native scalar accumulation differs from mathematical contract");
    Memory memory(image);const auto before=memory.ram;rt::I960Ref original(&memory);
    original.m_AC=0x3f001000;original.m_r[11]=cases::bits(c.accumulator);original.m_r[28]=cases::bits(c.metric);original.m_r[21]=cases::bits(c.sign);
    original.m_IP=0x5d3b48;original.execute_one();++instructions;
    original.m_IP=0x5d3b5c;unsigned count=0;
    while(original.m_IP!=0x5d3b84){if(++count>10)throw rt::Fatal("scalar region exceeded instruction bound");original.execute_one();}
    if(original.m_r[11]!=cases::bits(native)||(captured&&original.m_r[11]!=*captured)||memory.ram!=before||!memory.reads.empty()||!memory.writes.empty())
        throw rt::Fatal("original/native/captured scalar accumulation differs");
    instructions+=count;cases::hash(fingerprint,original.m_r[11]);++steps;if(captured)++live_steps;
}
void update(const std::vector<std::uint8_t>& image,const cases::Case& c,const cases::Words* captured=nullptr){
    auto native=c.state;if(!ps2::native::apply_contact_aggregate(c.input,native)||cases::words(native)!=cases::words(cases::mathematical(c)))
        throw rt::Fatal("native aggregate update differs from mathematical contract");
    Memory memory(image);
    for(unsigned i=0;i<3;++i){memory.at(position+4*i)=cases::bits(c.state.candidate_position[i]);memory.at(orientation+4*i)=cases::bits(c.state.candidate_orientation[i]);}
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<3;++j)memory.at(frame+0xa0+12*i+4*j)=cases::bits(c.input.responses[i][j]);
    constexpr std::array<unsigned,3> motion_offsets{0x2c,0x34,0x48},base_offsets{0x14,0x1c,0x3c};
    for(unsigned i=0;i<3;++i){memory.at(node+motion_offsets[i])=cases::bits(c.state.motion[i]);memory.at(node+base_offsets[i])=cases::bits(c.input.bases[i]);}
    memory.at(node+0x50)=cases::bits(c.input.response_divisor);memory.at(node+0x58)=cases::bits(c.input.scalar_divisor);
    memory.at(frame+0xf0)=orientation;memory.at(frame+0x100)=position;
    memory.enable=c.input.feedback_enabled?0u:0x91u;
    for(unsigned i=0;i<2;++i)memory.feedback[i]=std::bit_cast<std::uint32_t>(c.state.feedback[i]);
    auto expected_ram=memory.ram;auto expected_feedback=memory.feedback;
    const auto set=[&](std::uint32_t address,std::uint32_t value){expected_ram[(address-base)/4]=value;};
    for(unsigned i=0;i<3;++i)set(node+motion_offsets[i],cases::bits(native.motion[i]));
    set(position,cases::bits(native.candidate_position[0]));set(position+8,cases::bits(native.candidate_position[2]));set(orientation+4,cases::bits(native.candidate_orientation[1]));
    set(frame+0x110,cases::bits(c.input.responses[2][2]));
    for(unsigned i=0;i<2;++i)expected_feedback[i]=std::bit_cast<std::uint32_t>(native.feedback[i]);
    rt::I960Ref original(&memory);original.m_AC=0x3f001000;original.m_r[11]=cases::bits(c.input.signed_scalar);original.m_r[13]=node;original.m_r[31]=frame;original.m_IP=0x5d3b90;
    unsigned count=0;while(original.m_IP!=0x5d3cc0){if(++count>75)throw rt::Fatal("aggregate exceeded instruction bound");original.execute_one();}
    std::vector<std::uint32_t> expected_writes{frame+0x110,node+0x48,orientation+4,node+0x2c,node+0x34,position,position+8};
    if(c.input.feedback_enabled){expected_writes.push_back(0x213964);if(native.feedback[0]&&native.feedback[1]<0)expected_writes.push_back(0x213964);expected_writes.push_back(0x213968);}
    if(memory.ram!=expected_ram||memory.feedback!=expected_feedback||memory.enable!=(c.input.feedback_enabled?0u:0x91u)||memory.writes!=expected_writes||
       (captured&&cases::words(native)!=*captured)){
        std::fprintf(stderr,"UPDATE case=%u\n",updates);
        for(unsigned i=0;i<memory.ram.size();++i)if(memory.ram[i]!=expected_ram[i])std::fprintf(stderr,"RAM %08x original=%08x native=%08x\n",base+4*i,memory.ram[i],expected_ram[i]);
        for(unsigned i=0;i<2;++i)if(memory.feedback[i]!=expected_feedback[i])std::fprintf(stderr,"FEEDBACK %u original=%08x native=%08x\n",i,memory.feedback[i],expected_feedback[i]);
        if(memory.writes!=expected_writes){for(auto a:memory.writes)std::fprintf(stderr,"WRITE %08x\n",a);for(auto a:expected_writes)std::fprintf(stderr,"EXPECTED %08x\n",a);}
        throw rt::Fatal("original/native/captured aggregate state or write footprint differs");
    }
    instructions+=count;cases::hash(fingerprint,cases::words(native));++updates;if(captured)++live_updates;
}
} // namespace
int main(int argc,char** argv){
    if(argc!=2&&argc!=3)return 2;
    try{
        if(std::fegetround()!=FE_TONEAREST)throw rt::Fatal("oracle requires nearest-even rounding");
        std::ifstream stream(argv[1],std::ios::binary);std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(stream),{}};
        if(image.size()!=2097152)throw rt::Fatal("wrong private program size");
        if(!cases::steps([&](const auto& c){step(image,c);return true;},true)||!cases::each([&](const auto& c){update(image,c);return true;},true))return 1;
        if(steps!=25728||updates!=18400||fingerprint!=0x390cbdf6408065cdull)throw rt::Fatal("aggregate synthetic coverage/fingerprint changed");
        if(argc==3){
            std::ifstream live(argv[2],std::ios::binary);const std::vector<std::uint8_t> records{std::istreambuf_iterator<char>(live),{}};
            constexpr std::array<std::uint8_t,8> magic{'A','G','G','R','R','E','C','1'};
            if(records.size()<16||!std::equal(magic.begin(),magic.end(),records.begin()))throw rt::Fatal("invalid aggregate record magic");
            const auto word=[&](std::size_t offset){return std::uint32_t(records.at(offset))|(std::uint32_t(records.at(offset+1))<<8)|
                (std::uint32_t(records.at(offset+2))<<16)|(std::uint32_t(records.at(offset+3))<<24);};
            const auto step_count=word(8),update_count=word(12);
            if(!step_count||!update_count||update_count>32||step_count!=4*update_count||records.size()!=16ull+16ull*step_count+164ull*update_count)
                throw rt::Fatal("invalid aggregate record counts/length");
            for(unsigned i=0;i<step_count;++i){
                const auto offset=16+16*i,captured=word(offset+12);
                step(image,{cases::value(word(offset)),cases::value(word(offset+4)),cases::value(word(offset+8))},&captured);
            }
            for(unsigned i=0;i<update_count;++i){
                const auto offset=16+16*step_count+164*i;
                const auto raw=[&](unsigned index){return word(offset+4*index);};
                cases::Case c{};c.input.signed_scalar=cases::value(raw(0));
                for(unsigned j=0;j<4;++j)for(unsigned k=0;k<3;++k)c.input.responses[j][k]=cases::value(raw(1+3*j+k));
                for(unsigned j=0;j<3;++j)c.input.bases[j]=cases::value(raw(13+j));
                c.input.response_divisor=cases::value(raw(16));c.input.scalar_divisor=cases::value(raw(17));
                if(raw(18)>1)throw rt::Fatal("invalid aggregate feedback flag");c.input.feedback_enabled=raw(18)!=0;
                for(unsigned j=0;j<3;++j){c.state.motion[j]=cases::value(raw(19+j));c.state.candidate_position[j]=cases::value(raw(22+j));c.state.candidate_orientation[j]=cases::value(raw(25+j));}
                for(unsigned j=0;j<2;++j)c.state.feedback[j]=std::bit_cast<std::int32_t>(raw(28+j));
                cases::Words captured;for(unsigned j=0;j<captured.size();++j)captured[j]=raw(30+j);
                update(image,c,&captured);
            }
        }
        std::printf("CONTACT_AGGREGATE_ORACLE PASS steps=%u updates=%u live_steps=%u live_updates=%u instructions=%llu hash=%016llx\n",steps,updates,live_steps,live_updates,(unsigned long long)instructions,(unsigned long long)fingerprint);
    }catch(const std::exception& error){std::fprintf(stderr,"contact aggregate oracle failed: %s\n",error.what());return 1;}
}
