#include "full_step_capture.h"
#include <bit>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ps2::offline {
namespace {
constexpr std::size_t limit=256u*1024u*1024u;
constexpr std::array<std::array<std::uint32_t,2>,2> regions{{{0x200000,0x40000},{0x500000,0x100000}}};
struct ExternalWord { std::uint32_t value=0,mask=0,pc=0;std::uint64_t instructions=0; };
struct Proof { std::uint32_t pc=0,fp=0;std::array<std::uint32_t,16> words{}; };
struct State {
    std::filesystem::path path;
    std::ofstream transactions,boundaries;
    std::map<std::uint32_t,ExternalWord> external;
    std::array<std::uint8_t,12> controls{};
    std::uint64_t control_frame=~std::uint64_t{},instructions=0,events=0,steps=0,current=0;
    std::uint64_t frame=0,last_frame=0,first_frame=0,entry_instructions=0;
    std::uint32_t object=0,fp=0,helper_fp=0,vector=0,output=0;
    Proof parent,helper;
    unsigned helper_phase=0;
    std::size_t bytes=0;
    bool seeded=false;
};
std::unique_ptr<State> state;
unsigned suppressed=0;
bool ordinary(std::uint32_t a,unsigned width) {
    for (const auto &region:regions)
        if (a>=region[0] && std::uint64_t(a)+width<=std::uint64_t(region[0])+region[1]) return true;
    return false;
}
void bounded(State &s,std::size_t n) {
    if (n>limit-s.bytes) throw std::runtime_error("full-step byte limit exceeded");
    s.bytes+=n;
}
void little(std::ostream &out,std::uint64_t value,unsigned bytes) {
    for (unsigned i=0;i<bytes;++i) out.put(char(value>>(8*i)));
}
void transaction(State &s,std::uint64_t instructions,std::uint32_t pc,std::uint32_t a,
                 std::uint32_t value,unsigned flags) {
    bounded(s,24);little(s.transactions,instructions,8);little(s.transactions,pc,4);
    little(s.transactions,a,4);little(s.transactions,value,4);little(s.transactions,flags,4);
    if (!s.transactions) throw std::runtime_error("cannot write full-step transactions");
    ++s.events;
}
template<class T> void integers(std::ostream &out,const T &values) {
    out<<'[';bool first=true;for (auto v:values) { if (!first) out<<',';first=false;out<<+v; }out<<']';
}
std::uint32_t word(const rt::Cpu &cpu,std::uint32_t a) {
    if ((a&3u)||!ordinary(a,4)) throw std::runtime_error("full-step snapshot outside ordinary RAM");
    return cpu.bus->read_dword(a);
}
Proof caller(const rt::Cpu &cpu) {
    Proof p;p.fp=cpu.m_r[0]&~63u;
    if (!ordinary(p.fp,64)) throw std::runtime_error("full-step caller outside ordinary RAM");
    const auto index=std::int64_t(cpu.m_rcache_pos)-1;
    if (index>=0 && index<rt::Cpu::I960_RCACHE_SIZE) {
        if (cpu.m_rcache_frame_addr[index]!=p.fp) throw std::runtime_error("full-step caller cache mismatch");
        for (unsigned i=0;i<16;++i) p.words[i]=cpu.m_rcache[index][i];
    } else for (unsigned i=0;i<16;++i) p.words[i]=word(cpu,p.fp+4*i);
    p.pc=p.words[2];if (!p.pc || (p.pc&3u)) throw std::runtime_error("full-step return unaligned");
    return p;
}
void proof(std::ostream &out,const Proof &p) {
    out<<"{\"pc\":"<<p.pc<<",\"frame_pointer\":"<<p.fp<<",\"words\":";
    integers(out,p.words);out<<'}';
}
void seed(State &s,const rt::Cpu &cpu) {
    std::ofstream file(s.path/"full-step-seed.bin.tmp",std::ios::binary);
    for (const auto &region:regions) for (std::uint32_t offset=0;offset<region[1];offset+=4)
        little(file,word(cpu,region[0]+offset),4);
    file.close();if (!file) throw std::runtime_error("cannot write initial full-step RAM seed");
    bounded(s,0x140000);s.seeded=true;
}
void snapshot(State &s,const rt::Cpu &cpu,const char *event,std::uint64_t ns) {
    std::ostringstream out;
    out<<"{\"schema\":1,\"event\":\""<<event<<"\",\"call_id\":"<<s.current
       <<",\"frame\":"<<s.frame<<",\"instructions\":"<<s.instructions<<",\"pc\":"<<cpu.m_IP
       <<",\"cycles\":"<<cpu.reference_cycles<<",\"board_ns\":"<<ns<<",\"ac\":"<<cpu.m_AC
       <<",\"processor_control\":"<<cpu.m_PC<<",\"object\":"<<s.object
       <<",\"vehicle_frame_pointer\":"<<s.fp<<",\"transaction_index\":"<<s.events
       <<",\"controls_frame\":"<<s.control_frame<<",\"controls\":";
    integers(out,s.controls);out<<",\"registers\":";integers(out,cpu.m_r);
    std::array<std::uint64_t,4> fp{};for (unsigned i=0;i<4;++i) fp[i]=std::bit_cast<std::uint64_t>(cpu.m_fp[i]);
    out<<",\"fp_bits\":";integers(out,fp);out<<",\"parent_return\":";proof(out,s.parent);
    if (s.helper_phase) { out<<",\"helper_return\":";proof(out,s.helper);out<<",\"helper_frame_pointer\":"<<s.helper_fp; }
    out<<",\"blocks\":[";bool first=true;
    auto block=[&](const char *name,std::uint32_t address,unsigned count) {
        if (!first) out<<',';first=false;
        out<<"{\"name\":\""<<name<<"\",\"address\":"<<address<<",\"words\":[";
        for (unsigned i=0;i<count;++i) { if (i) out<<',';out<<word(cpu,address+4*i); }out<<"]}";
    };
    block("object",s.object,64);
    // Independently derived whole-37870 state closure, retained at every parent
    // and helper boundary. These are raw words, not inferred physical units.
    constexpr std::array<std::array<std::uint32_t,2>,18> globals{{
        {0x202008,1},{0x202018,1},{0x202044,4},{0x202098,1},{0x2020b4,1},{0x20b0b4,1},
        {0x2139cc,2},{0x214120,2},{0x2141c0,13},{0x214200,2},{0x214208,1},{0x21420c,4},
        {0x214230,10},{0x214258,4},{0x214270,26},{0x2142e0,21},{0x2197b0,1},{0x2140cc,1}}};
    for (const auto &g:globals) block("global",g[0],g[1]);
    if (s.helper_phase) { block("balance_vector",s.vector,3);block("balance_output",s.output,3); }
    out<<"]}\n";const auto text=out.str();bounded(s,text.size());s.boundaries<<text;
    if (!s.boundaries) throw std::runtime_error("cannot write full-step boundary");
}
} // namespace
SuppressStepAccess::SuppressStepAccess() { ++suppressed; }
SuppressStepAccess::~SuppressStepAccess() { --suppressed; }
void begin_full_step_capture(const char *directory) {
    if (state) throw std::runtime_error("full-step capture already active");
    const auto root=std::filesystem::canonical(__FILE__).parent_path().parent_path().parent_path().parent_path();
    const auto build=root/"build";
    if (std::filesystem::is_symlink(build)) throw std::runtime_error("full-step build root is redirected");
    const auto path=std::filesystem::canonical(directory);
    const auto relative=path.lexically_relative(build);
    if (relative.empty()||relative=="."||*relative.begin()=="..") throw std::runtime_error("full-step output must remain in build");
    for (const auto *name:{"full-step-access.bin","full-step.jsonl","full-step-seed.bin","full-step-summary.json"})
        if (std::filesystem::exists(path/name)||std::filesystem::exists(path/(std::string(name)+".tmp")))
            throw std::runtime_error("full-step output already exists");
    auto s=std::make_unique<State>();s->path=path;
    s->transactions.open(path/"full-step-access.bin.tmp",std::ios::binary);
    s->boundaries.open(path/"full-step.jsonl.tmp");
    if (!s->transactions||!s->boundaries) throw std::runtime_error("cannot open full-step streams");
    s->transactions.write("RLYSTEP1",8);s->bytes=8;state=std::move(s);
}
void full_step_controls(std::uint64_t frame,const std::array<std::uint8_t,12> &controls) {
    if (!state) return;
    if (state->current) throw std::runtime_error("cabinet inputs changed within callback");
    state->controls=controls;state->control_frame=frame;
}
void full_step_instruction(std::uint64_t instructions) { if (state) state->instructions=instructions; }
void full_step_access(const rt::Cpu *cpu,std::uint32_t address,std::uint32_t value,unsigned width,bool writing) {
    if (!state||suppressed||!state->seeded) return;
    if (!cpu||(width!=1&&width!=2&&width!=4)||std::uint64_t(address)+width>0x100000000ull)
        throw std::runtime_error("invalid full-step bus transaction");
    auto &s=*state;
    if (s.current) {
        transaction(s,s.instructions,cpu->m_IP,address,value,width|(writing?0x100u:0u));
    } else if (writing && ordinary(address,width)) {
        for (unsigned i=0;i<width;++i) {
            const auto a=address+i,shift=8*(a&3u);auto &word=s.external[a&~3u];
            word.value=(word.value&~(255u<<shift))|(((value>>(8*i))&255u)<<shift);
            word.mask|=1u<<(a&3u);word.pc=cpu->m_IP;word.instructions=s.instructions;
        }
        if (s.external.size()>0x50000) throw std::runtime_error("external RAM patch exceeded address space");
    }
}
void full_step_boundary(const rt::Cpu &cpu,const char *event,std::uint64_t id,std::uint32_t object,
                        std::uint64_t instructions,std::uint64_t frame,std::uint64_t ns) {
    if (!state) return;
    SuppressStepAccess guard;auto &s=*state;s.instructions=instructions;
    const std::string_view name(event);
    if (name=="vehicle_entry") {
        if (s.current||s.steps>=1201||!ordinary(object,256)||!ordinary(cpu.m_r[31],0x1f4)||
            s.control_frame!=frame || (s.steps && (frame!=s.last_frame+1||object!=s.object)))
            throw std::runtime_error("full-step entry is not one consecutive controlled callback");
        if (!s.seeded) { seed(s,cpu);s.first_frame=frame; }
        for (const auto &[a,w]:s.external) {
            if (w.mask==15) transaction(s,w.instructions,w.pc,a,w.value,4|0x300);
            else for (unsigned i=0;i<4;++i) if (w.mask&(1u<<i))
                transaction(s,w.instructions,w.pc,a+i,(w.value>>(8*i))&255u,1|0x300);
        }
        s.external.clear();s.current=id;s.object=object;s.fp=cpu.m_r[31];s.parent=caller(cpu);
        s.frame=frame;s.entry_instructions=instructions;s.helper_phase=0;
        if (cpu.m_IP!=0x5d3f40||s.parent.pc!=0x5c9bac) throw std::runtime_error("full-step original callback identity mismatch");
    } else {
        if (!s.current||s.current!=id||object!=s.object||frame!=s.frame||instructions-s.entry_instructions>2000000)
            throw std::runtime_error("full-step boundary lost parent identity");
        if (name=="balance_helper_pre") {
            if (s.helper_phase||cpu.m_IP!=0x5d6870||cpu.m_r[16]!=object||!ordinary(cpu.m_r[17],12)||!ordinary(cpu.m_r[18],12))
                throw std::runtime_error("full-step balance entry mismatch");
            s.helper=caller(cpu);s.helper_fp=cpu.m_r[31];s.vector=cpu.m_r[17];s.output=cpu.m_r[18];s.helper_phase=1;
            if (s.helper.pc!=0x5d5760||s.helper.fp!=s.fp) throw std::runtime_error("full-step helper caller mismatch");
        } else if (name=="balance_helper_post") {
            if (s.helper_phase!=1||cpu.m_IP!=s.helper.pc||cpu.m_r[31]!=s.fp)
                throw std::runtime_error("full-step balance return mismatch");
            s.helper_phase=2;
        } else if (name=="vehicle_return") {
            if (s.helper_phase!=2||cpu.m_IP!=s.parent.pc||cpu.m_r[31]!=s.parent.fp)
                throw std::runtime_error("full-step return mismatch");
        } else throw std::runtime_error("unknown full-step boundary");
    }
    snapshot(s,cpu,event,ns);
    if (name=="vehicle_return") { ++s.steps;s.last_frame=frame;s.current=0; }
}
void end_full_step_capture() {
    if (!state) return;
    auto &s=*state;
    if (s.current||!s.steps||s.steps!=s.last_frame-s.first_frame+1) throw std::runtime_error("incomplete full-step capture");
    s.transactions.close();s.boundaries.close();
    if (!s.transactions||!s.boundaries) throw std::runtime_error("cannot finish full-step files");
    std::ofstream summary(s.path/"full-step-summary.json.tmp");
    summary<<"{\"schema\":1,\"complete\":true,\"steps\":"<<s.steps<<",\"first_frame\":"<<s.first_frame
           <<",\"last_frame\":"<<s.last_frame<<",\"transactions\":"<<s.events<<",\"bytes\":"<<s.bytes
           <<",\"record_bytes\":24,\"seed_regions\":[[2097152,262144],[5242880,1048576]],"
           <<"\"scope\":\"Actual CPU data accesses; fetches and snapshot reads excluded. Device internal state is not seeded. Between-step patches are last ordinary writes from other CPU code, not assumed controls.\","
           <<"\"native_updates_compared\":false}\n";
    summary.close();if (!summary) throw std::runtime_error("cannot finish full-step summary");
    for (const auto *name:{"full-step-access.bin","full-step.jsonl","full-step-seed.bin","full-step-summary.json"})
        std::filesystem::rename(s.path/(std::string(name)+".tmp"),s.path/name);
    state.reset();
}
} // namespace ps2::offline
