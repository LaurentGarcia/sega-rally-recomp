// Offline whole-helper comparison. Original instructions/assets stay private.
#include "segment_selection_cases.h"
#include "refcore/i960_ref.h"
#include "runtime/tgp.h"
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
namespace {
namespace cases=segment_selection_cases;
using Bytes=std::vector<std::uint8_t>;
Bytes read(const char* path){std::ifstream f(path,std::ios::binary);if(!f)throw rt::Fatal("missing private input");return {std::istreambuf_iterator<char>(f),{}};}
std::uint32_t word(const Bytes& b,std::size_t o){return b.at(o)|(std::uint32_t(b.at(o+1))<<8)|(std::uint32_t(b.at(o+2))<<16)|(std::uint32_t(b.at(o+3))<<24);}
struct Geometry final:rt::TgpBus {
    std::vector<std::uint32_t> input,output;unsigned consumed=0;
    bool fifo_pop(std::uint32_t& v)override{if(consumed==input.size())return false;v=input[consumed++];return true;}
    void fifo_push(std::uint32_t v)override{output.push_back(v);}
    std::uint32_t mem_r(std::uint32_t)override{throw rt::TgpFatal("unexpected segment external geometry read");}
    void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("unexpected segment external geometry write");}
    void bank_w(std::uint32_t v)override{if(v)throw rt::TgpFatal("unexpected segment geometry bank");}
};
struct Done{};
void hook(rt::Tgp& t,std::uint16_t pc){
    if(pc<0x577||pc>=0x58a||t.count>19)throw rt::TgpFatal("segment edge escaped verified region");
    if(pc==0x589){if(t.pc!=0xfff)throw rt::TgpFatal("wrong segment edge return");throw Done{};}
}
constexpr std::uint32_t ram_base=0x220000,point=ram_base+0x800,index_pair=ram_base+0x900;
constexpr std::uint32_t vertex_base=0x240000,header=0x260000,record_base=0x300000;
constexpr std::uint32_t entry=0x5ca0b0,done=0x123400;
struct Memory final:rt::Bus {
    const Bytes& image;const cases::Case& c;rt::Tgp tgp;
    std::array<std::uint32_t,1024> ram{};
    std::vector<std::uint32_t> arguments,reply,half_writes,visited;
    unsigned consumed=0,commands=0,selector=0;bool collecting=false;
    std::uint64_t geometry_instructions=0;
    Memory(const Bytes& p,const rt::Tgp& initial,const cases::Case& input,unsigned choice):image(p),c(input),tgp(initial),selector(choice){
        for(unsigned i=0;i<ram.size();++i)ram[i]=0xa5730000u^i;
        at(point)=std::bit_cast<std::uint32_t>(c.point.x);at(point+4)=0x7fa12345u;at(point+8)=std::bit_cast<std::uint32_t>(c.point.z);
        at(index_pair)=0x3412u|(std::uint32_t(std::bit_cast<std::uint16_t>(c.progression))<<16);
    }
    std::uint32_t& at(std::uint32_t a){if(a<ram_base||a>=ram_base+ram.size()*4||(a&3))throw rt::Fatal("segment RAM outside fixture");return ram[(a-ram_base)/4];}
    std::uint32_t fetch(std::uint32_t a)override{if(a<0x5c9ed0||a>=0x5ca174||(a&3))throw rt::Fatal("segment fetch escaped verified helpers");return word(image,a-0x59f000);}
    std::uint32_t read_dword(std::uint32_t a)override{
        if(a==0x884000){if(collecting||consumed>=reply.size())throw rt::Fatal("unexpected segment FIFO read");return reply[consumed++];}
        if(a==0x214354)return selector;
        if(a==0x5dcac8+12*selector)return header;
        if(a==header)return std::uint32_t(c.segments.size());if(a==header+4)return vertex_base;if(a==header+8)return record_base;
        if(a>=vertex_base&&a<vertex_base+12*c.vertices.size()&&!(a&3)){
            const unsigned index=(a-vertex_base)/12,component=((a-vertex_base)%12)/4;
            return component==1?(0x7fa00000u|index):std::bit_cast<std::uint32_t>(component?c.vertices[index].z:c.vertices[index].x);
        }
        if(a>=ram_base&&a<ram_base+ram.size()*4)return at(a);
        throw rt::Fatal("segment read escaped supplied assets/state");
    }
    void write_dword(std::uint32_t a,std::uint32_t v)override{
        if(a==0x884000){
            if(!collecting){if(consumed!=reply.size()||v!=0x2e005c5cu)throw rt::Fatal("unexpected segment geometry command");arguments.clear();reply.clear();consumed=0;collecting=true;return;}
            arguments.push_back(v);if(arguments.size()==6){
                Geometry bus;bus.input=arguments;tgp.bus=&bus;tgp.pc=0x577;tgp.pcs[0]=0xfff;tgp.count=0;tgp.hook=hook;
                try{rt::tgpgen::run(tgp,20);throw rt::Fatal("segment edge did not return");}catch(Done&){}
                if(bus.consumed!=6||bus.output.size()!=1||tgp.tables||tgp.bank_reg)throw rt::Fatal("segment edge transfer mismatch");
                geometry_instructions+=tgp.count;reply=bus.output;++commands;collecting=false;
            }return;
        }
        // The callee stages five triples in its own frame. Fourth words loaded
        // by its burst reads are deliberate unchanged fixture guards.
        if(a<ram_base+0x180||a>ram_base+0x1c8||((a-(ram_base+0x180))%16)>8||(a&3))
            throw rt::Fatal("segment write escaped private callee scratch");
        at(a)=v;
    }
    std::uint16_t read_word(std::uint32_t a)override{
        if(a==index_pair||a==index_pair+2)return std::uint16_t(at(index_pair)>>(8*(a-index_pair)));
        if(a>=record_base&&a<record_base+12*c.segments.size()){
            const auto offset=a-record_base;const unsigned part=offset%12;
            if(part>=4&&!(part&1))return c.segments[offset/12][(part-4)/2];
        }throw rt::Fatal("segment halfword read escaped corner indices");
    }
    void write_word(std::uint32_t a,std::uint16_t v)override{
        if(a!=index_pair&&a!=index_pair+2)throw rt::Fatal("segment halfword write escaped indices");
        const unsigned shift=8*(a-index_pair);at(index_pair)=(at(index_pair)&~(0xffffu<<shift))|(std::uint32_t(v)<<shift);half_writes.push_back(a);
    }
    std::uint8_t read_byte(std::uint32_t)override{throw rt::Fatal("unexpected segment byte read");}
    void write_byte(std::uint32_t,std::uint8_t)override{throw rt::Fatal("unexpected segment byte write");}
    std::uint16_t flags(std::uint32_t)override{return rt::Cpu::BURST;}
};
unsigned count=0,live=0,hits=0;std::uint64_t fingerprint=14695981039346656037ull,cpu_instructions=0,geometry_instructions=0;
void compare(const Bytes& image,const rt::Tgp& initial,const cases::Case& c,const std::array<std::uint32_t,3>* captured=nullptr){
    ps2::native::SegmentSelection native;
    if(!ps2::native::select_course_segment(c.vertices,c.segments,c.point,c.progression,native)||cases::words(native)!=cases::words(cases::mathematical(c)))throw rt::Fatal("segment native/formula mismatch");
    Memory memory(image,initial,c,count%8);const auto before=memory.ram;
    rt::I960Ref cpu(&memory);cpu.m_AC=0x3f001000;cpu.m_IP=done;cpu.m_r[31]=ram_base;cpu.m_r[1]=ram_base+0x100;
    cpu.m_r[16]=point;cpu.m_r[17]=index_pair;cpu.do_call(entry,0,0);
    unsigned steps=0;while(cpu.m_IP!=done){
        if(++steps>100000u+200u*c.segments.size())throw rt::Fatal("segment CPU instruction bound exceeded");
        if(cpu.m_IP==0x5c9ed0){if(cpu.m_r[16]!=vertex_base||cpu.m_r[18]!=point||cpu.m_r[17]<record_base||(cpu.m_r[17]-record_base)%12)throw rt::Fatal("segment callee arguments differ");memory.visited.push_back((cpu.m_r[17]-record_base)/12);}
        cpu.execute_one();
    }
    const auto out=memory.at(index_pair);const std::array<std::uint32_t,3> actual{cpu.m_r[16],out&0xffffu,out>>16};
    if(actual!=cases::words(native)||(captured&&actual!=*captured)||memory.commands!=4*memory.visited.size()||memory.collecting||memory.consumed!=memory.reply.size())throw rt::Fatal("segment original/native/captured result mismatch");
    const std::vector<std::uint32_t> expected_writes=native.selected<0?std::vector<std::uint32_t>{index_pair+2,index_pair}:std::vector<std::uint32_t>{index_pair,index_pair+2};
    if(memory.half_writes!=expected_writes||cpu.m_r[31]!=ram_base||cpu.m_rcache_pos!=0)throw rt::Fatal("segment return/write contract mismatch");
    const int n=int(c.segments.size());int center=int(c.progression)%n;if(center<0)center+=n;
    for(unsigned i=0;i<memory.visited.size();++i){const int delta=i%2?int(i/2)+1:-int(i/2);
        if(memory.visited[i]!=unsigned((center+n+delta)%n))throw rt::Fatal("segment visit order mismatch");}
    if(memory.visited.empty()||memory.visited.size()>c.segments.size()||(native.selected<0&&memory.visited.size()!=c.segments.size())||
       (native.selected>=0&&memory.visited.back()!=unsigned(native.selected)))throw rt::Fatal("segment first-match bound mismatch");
    for(unsigned i=0;i<before.size();++i){const auto a=ram_base+4*i;
        if(a==index_pair||(a>=ram_base+0x180&&a<=ram_base+0x1c8&&(a-(ram_base+0x180))%16<=8))continue;
        if(before[i]!=memory.ram[i])throw rt::Fatal("segment changed preserved memory");}
    ++count;live+=captured!=nullptr;hits+=native.selected>=0;cpu_instructions+=steps;geometry_instructions+=memory.geometry_instructions;cases::hash(fingerprint,native);
}
cases::Case asset(const Bytes& image,const Bytes& data,unsigned selector){
    if(selector>=8)throw rt::Fatal("segment descriptor outside verified window");
    const auto h=word(image,0x3dac8+12*selector);if(h<0x2000000||h-0x2000000>data.size()-12)throw rt::Fatal("segment header outside private image");
    const auto o=h-0x2000000;const auto n=word(data,o),v=word(data,o+4),r=word(data,o+8);
    if(!n||n>65535||r<0x2000000||r-0x2000000>data.size()-12ull*n||v<0x2000000)throw rt::Fatal("segment asset extent invalid");
    cases::Case c;unsigned max_index=0;
    for(unsigned i=0;i<n;++i){ps2::native::CourseSegmentCorners q;for(unsigned j=0;j<4;++j){const auto a=r-0x2000000+12*i+4+2*j;q[j]=std::uint16_t(data.at(a)|(unsigned(data.at(a+1))<<8));max_index=std::max(max_index,unsigned(q[j]));}c.segments.push_back(q);}
    if(v-0x2000000>data.size()-12ull*(max_index+1))throw rt::Fatal("segment vertices outside private image");
    for(unsigned i=0;i<=max_index;++i){const auto a=v-0x2000000+12*i;c.vertices.push_back({std::bit_cast<float>(word(data,a)),std::bit_cast<float>(word(data,a+8))});}
    return c;
}
}
int main(int argc,char**argv){try{
    if(argc!=3&&argc!=5)return 2;if(std::fegetround()!=FE_TONEAREST)throw rt::Fatal("segment oracle requires nearest-even");
    const auto image=read(argv[1]),firmware=read(argv[2]);if(image.size()!=2097152||firmware.size()!=6660)throw rt::Fatal("wrong private image size");
    rt::Tgp initial;Geometry boot;initial.reset();initial.bus=&boot;for(unsigned i=0;i<firmware.size()/4;++i)initial.prog[i]=word(firmware,4*i);
    boot.input={0x04000000};rt::tgpgen::run(initial,100000);initial.bus=nullptr;
    if(initial.count>=100000||boot.consumed!=1||boot.output!=std::vector<std::uint32_t>{8}||initial.tables||initial.bank_reg)throw rt::Fatal("segment firmware startup failed");
    if(!cases::each([&](const auto& c){compare(image,initial,c);return true;},false))return 1;
    if(count!=3634||fingerprint!=0x1c1c549c187f8285ull)throw rt::Fatal("segment synthetic coverage changed");
    if(argc==5){const auto data=read(argv[3]),records=read(argv[4]);constexpr std::array<std::uint8_t,8> magic{'S','E','G','M','E','N','T','1'};
        if(data.size()!=37748736||records.size()<12||!std::equal(magic.begin(),magic.end(),records.begin()))throw rt::Fatal("invalid segment private asset/records");
        const unsigned n=word(records,8);if(!n||n>64||records.size()!=12ull+28ull*n)throw rt::Fatal("invalid segment record count");
        for(unsigned i=0;i<n;++i){const auto o=12+28*i;auto c=asset(image,data,word(records,o));
            c.point={std::bit_cast<float>(word(records,o+4)),std::bit_cast<float>(word(records,o+8))};c.progression=std::bit_cast<std::int16_t>(std::uint16_t(word(records,o+12)));
            std::array<std::uint32_t,3> expected{word(records,o+16),word(records,o+20),word(records,o+24)};compare(image,initial,c,&expected);}
    }
    std::printf("SEGMENT_SELECTION_ORACLE PASS cases=%u live=%u hits=%u cpu_instructions=%llu geometry_instructions=%llu hash=%016llx\n",count,live,hits,(unsigned long long)cpu_instructions,(unsigned long long)geometry_instructions,(unsigned long long)fingerprint);
}catch(const std::exception& e){std::fprintf(stderr,"segment oracle failed: %s\n",e.what());return 1;}return 0;}
