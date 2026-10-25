// Host-only bounded original-program comparison; excluded from native builds.
#include "contact_normalization_cases.h"
#include "refcore/i960_ref.h"
#include <fstream>
#include <iterator>
#include <set>
#include <vector>
#include <cstdio>
#include <sstream>
#include <cfenv>
namespace {
namespace cases=contact_normalization_cases;
using namespace ps2::native;
constexpr std::uint32_t entry=0x5d38ec,end=0x5d3968,frame=0x210000,base=frame+0xd0;
struct Memory final:rt::Bus {
    const std::vector<std::uint8_t> &program;
    cases::Words words{};
    unsigned reads=0,writes=0;
    explicit Memory(const std::vector<std::uint8_t>&p):program(p){}
    std::uint32_t fetch(std::uint32_t a)override {
        if(a<entry||a>=end||(a&3))throw rt::Fatal("normalization fetch escaped verified region");
        std::uint32_t w=0;for(unsigned i=0;i<4;++i)w|=std::uint32_t(program.at(a-0x59f000+i))<<(8*i);return w;
    }
    unsigned slot(std::uint32_t a) {
        if(a<base||a>=base+12||(a&3))throw rt::Fatal("normalization access escaped vector");
        return (a-base)/4;
    }
    std::uint32_t read_dword(std::uint32_t a)override {const auto i=slot(a);if(reads>=5||i!=(reads<3?reads:reads-2))throw rt::Fatal("normalization read order changed");++reads;return words[i];}
    void write_dword(std::uint32_t a,std::uint32_t v)override {
        const auto i=slot(a);if(i!=writes++)throw rt::Fatal("normalization write order changed");words[i]=v;
    }
    std::uint8_t read_byte(std::uint32_t)override{throw rt::Fatal("unexpected byte read");}
    std::uint16_t read_word(std::uint32_t)override{throw rt::Fatal("unexpected word read");}
    void write_byte(std::uint32_t,std::uint8_t)override{throw rt::Fatal("unexpected byte write");}
    void write_word(std::uint32_t,std::uint16_t)override{throw rt::Fatal("unexpected word write");}
    std::uint16_t flags(std::uint32_t)override{return rt::Cpu::BURST;}
};
unsigned count=0,instructions=0,fallbacks=0,live=0;
std::uint64_t hash=14695981039346656037ull;
std::set<cases::Words> unique;
bool compare(const std::vector<std::uint8_t>&program,const cases::Words &input,const cases::Words *expected=nullptr) {
    ContactDirection3 output;
    if(!normalize_contact_direction(cases::vector(input),output))throw rt::Fatal("native normalization rejected fixture");
    const auto result=cases::bits(output);
    Memory memory(program);memory.words=input;
    rt::I960Ref cpu(&memory);for(auto &r:cpu.m_r)r=0xa5a5a5a5u;
    cpu.m_r[7]=base;cpu.m_r[25]=base+8;cpu.m_r[31]=frame;cpu.m_r[30]=0;
    cpu.m_AC=0x3f001000u;cpu.m_IP=entry;
    unsigned steps=0;bool fallback=false;
    while(cpu.m_IP!=end){if(++steps>40)throw rt::Fatal("normalization instruction bound exceeded");if(cpu.m_IP==0x5d3954)fallback=true;cpu.execute_one();}
    if(steps!=(fallback?20u:24u)||memory.words!=result||memory.writes!=3||memory.reads!=(fallback?3u:5u))
        {std::fprintf(stderr,"case=%u in=%08x,%08x,%08x native=%08x,%08x,%08x original=%08x,%08x,%08x reads=%u writes=%u steps=%u\n",count,input[0],input[1],input[2],result[0],result[1],result[2],memory.words[0],memory.words[1],memory.words[2],memory.reads,memory.writes,steps);throw rt::Fatal("original normalization value/access mismatch");}
    if(expected&&result!=*expected)throw rt::Fatal("live normalization output mismatch");
    if(fallback)++fallbacks;
    unique.insert(input);cases::hash(hash,input,result);++count;instructions+=steps;return true;
}
}
int main(int argc,char **argv) {
    if(argc!=2&&argc!=3)return 2;
    try {
        if(std::fegetround()!=FE_TONEAREST)throw rt::Fatal("normalization oracle requires nearest-even host rounding");
        std::ifstream stream(argv[1],std::ios::binary);
        const std::vector<std::uint8_t> program{std::istreambuf_iterator<char>(stream),{}};
        if(program.size()!=2097152)throw rt::Fatal("wrong program size");
        if(!cases::each([&](const auto &c){return compare(program,c);},true))return 1;
        if(count!=59912||hash!=0x53b71c272e0a45eaull)throw rt::Fatal("normalization coverage changed");
        if(argc==3) {
            std::ifstream rows(argv[2]);if(!rows)throw rt::Fatal("missing live rows");
            std::string line;
            while(std::getline(rows,line)) {
                std::istringstream row(line);cases::Words in,out;row>>std::hex;
                for(auto &w:in)if(!(row>>w))throw rt::Fatal("bad live input row");
                for(auto &w:out)if(!(row>>w))throw rt::Fatal("bad live output row");
                std::string extra;if(row>>extra)throw rt::Fatal("extra live row fields");
                compare(program,in,&out);++live;
            }
            if(!live)throw rt::Fatal("empty live normalization rows");
        }
        std::printf("CONTACT_NORMALIZATION_ORACLE PASS cases=%u unique_inputs=%zu instructions=%u fallback_outputs=%u live=%u hash=%016llx\n",
            count,unique.size(),instructions,fallbacks,live,(unsigned long long)hash);
    }catch(const std::exception &error){std::fprintf(stderr,"normalization oracle failed: %s\n",error.what());return 1;}
}
