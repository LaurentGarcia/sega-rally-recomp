#include "visual_camera_capture.h"
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace ps2::offline {
namespace {
struct Saved {
    std::uint32_t pc{},fp{};
    int cache=-1;
    std::array<std::uint32_t,16> words{};
};
struct Call {
    unsigned id{},entry{};
    std::uint32_t fp{},pose{},descriptor{};
    Saved saved;
};
struct Capture {
    std::filesystem::path directory;
    std::ofstream records;
    unsigned first{},last{},calls{},completed{},events{};
    std::size_t bytes{};
    std::uint64_t last_instruction=~std::uint64_t{};
    std::vector<Call> stack;
    std::map<std::uint32_t,std::uint64_t> coverage;
};
std::unique_ptr<Capture> active;
bool ordinary(std::uint32_t address,unsigned count) {
    const auto end=std::uint64_t(address)+4ull*count;
    return !(address&3)&&count<=64&&((address>=0x200000&&end<=0x240000)||(address>=0x500000&&end<=0x600000));
}
std::vector<std::uint32_t> words(const rt::Cpu&cpu,std::uint32_t address,unsigned count) {
    if(!ordinary(address,count))throw std::runtime_error("visual camera observation outside ordinary RAM");
    std::vector<std::uint32_t> result;
    for(unsigned i=0;i<count;++i)result.push_back(cpu.bus->read_dword(address+4*i));
    return result;
}
template<class T> void list(std::ostream&out,const T&v) {
    out<<'[';bool first=true;
    for(auto x:v){if(!first)out<<',';first=false;out<<x;}out<<']';
}
Saved saved(const rt::Cpu&cpu) {
    Saved s;s.fp=cpu.m_r[0]&~0x3fu;
    if(!ordinary(s.fp,16))throw std::runtime_error("visual caller frame outside RAM");
    const auto index=std::int64_t(cpu.m_rcache_pos)-1;
    if(index>=0&&index<rt::Cpu::I960_RCACHE_SIZE){
        s.cache=int(index);
        if(cpu.m_rcache_frame_addr[index]!=s.fp)throw std::runtime_error("visual caller cache identity differs");
        for(unsigned i=0;i<16;++i)s.words[i]=cpu.m_rcache[index][i];
    }else{auto v=words(cpu,s.fp,16);for(unsigned i=0;i<16;++i)s.words[i]=v[i];}
    s.pc=s.words[2];
    if(!s.pc||(s.pc&3))throw std::runtime_error("visual caller return is invalid");
    return s;
}
void record(Capture&c,const Call&call,const rt::Cpu&cpu,std::uint64_t instruction,
            std::uint64_t frame,std::uint64_t ns,const char*kind) {
    if(++c.events>2048)throw std::runtime_error("visual camera event budget exceeded");
    std::ostringstream out;
    out<<"{\"schema\":1,\"event\":\""<<kind<<"\",\"call_id\":"<<call.id
       <<",\"entry_pc\":"<<call.entry<<",\"pc\":"<<cpu.m_IP<<",\"frame\":"<<frame
       <<",\"instructions\":"<<instruction<<",\"board_ns\":"<<ns<<",\"cycles\":"<<cpu.reference_cycles
       <<",\"ac\":"<<cpu.m_AC<<",\"frame_pointer\":"<<call.fp<<",\"registers\":";
    list(out,cpu.m_r);
    std::array<std::uint64_t,4> fp{};for(unsigned i=0;i<4;++i)fp[i]=std::bit_cast<std::uint64_t>(cpu.m_fp[i]);
    out<<",\"fp_bits\":";list(out,fp);
    out<<",\"return_frame\":{\"pc\":"<<call.saved.pc<<",\"frame_pointer\":"<<call.saved.fp
       <<",\"cache_index\":"<<call.saved.cache<<",\"source\":\""<<(call.saved.cache<0?"ordinary_ram":"register_cache")<<"\",\"words\":";
    list(out,call.saved.words);out<<"},\"blocks\":[";
    bool first=true;
    const auto block=[&](const char*name,std::uint32_t address,unsigned count){
        if(!first)out<<',';
        first=false;
        out<<"{\"name\":\""<<name<<"\",\"address\":"<<address<<",\"words\":";
        list(out,words(cpu,address,count));out<<'}';
    };
    block("main_pose",0x213b40,22);block("view",0x202200,6);block("focus",0x202270,2);
    block("view_mode",0x2140c0,1);block("view_dispatch",0x20ab4c,5);block("pose_pointers",0x213980,16);
    block("view_parameters",0x20ac70,4);
    if(call.pose)block("draw_pose",call.pose,22);
    if(call.entry==0x1ff60&&cpu.m_IP==0x5bf1f0)block("camera_matrix",call.fp+0x60,12);
    if(call.descriptor){
        block("draw_descriptor",call.descriptor,12);
        const auto wheel=words(cpu,call.descriptor,1)[0];
        if(ordinary(wheel,12))block("wheel_offsets",wheel,12);
    }
    out<<"]}";
    const auto text=out.str();c.bytes+=text.size()+1;
    if(c.bytes>8*1024*1024)throw std::runtime_error("visual camera byte budget exceeded");
    c.records<<text<<'\n';if(!c.records)throw std::runtime_error("cannot write visual camera record");
}
}
void begin_visual_camera_capture(const char*directory,unsigned first,unsigned last) {
    if(active||!directory||!*directory||!first||last<first||last-first>8)throw std::runtime_error("invalid visual capture range");
    namespace fs=std::filesystem;
    const auto root=fs::canonical(__FILE__).parent_path().parent_path().parent_path().parent_path();
    if(fs::is_symlink(root/"build"))throw std::runtime_error("visual build root is a symlink");
    const auto build=fs::canonical(root/"build"),path=fs::weakly_canonical(fs::absolute(directory));
    auto b=build.begin(),p=path.begin();for(;b!=build.end()&&p!=path.end()&&*b==*p;++b,++p){}
    if(b!=build.end()||p==path.end()||fs::exists(path))throw std::runtime_error("visual capture requires new private build directory");
    fs::create_directories(path);auto c=std::make_unique<Capture>();c->directory=path;c->first=first;c->last=last;
    c->records.open(path/"camera.jsonl.tmp");if(!c->records)throw std::runtime_error("cannot open visual camera records");
    active=std::move(c);
}
void observe_visual_camera(const rt::Cpu&cpu,std::uint64_t instructions,std::uint64_t frame,std::uint64_t ns) {
    if(!active)return;
    auto&c=*active;
    if(frame<c.first||frame>c.last){if(frame>c.last&&!c.stack.empty())throw std::runtime_error("visual calls cross capture frame boundary");return;}
    if(instructions==c.last_instruction)return;
    c.last_instruction=instructions;
    const auto pc=cpu.m_IP>=0x59f000&&cpu.m_IP<0x600000?cpu.m_IP-0x59f000:cpu.m_IP;
    if(pc>=0x1f500&&pc<0x20390)++c.coverage[pc];
    if(pc==0x201f0&&!c.stack.empty()&&c.stack.back().entry==0x1ff60){
        if(cpu.m_r[31]!=c.stack.back().fp)throw std::runtime_error("visual camera matrix frame differs");
        record(c,c.stack.back(),cpu,instructions,frame,ns,"view_matrix");
    }
    if(!c.stack.empty()&&cpu.m_IP==c.stack.back().saved.pc&&cpu.m_r[31]==c.stack.back().saved.fp){
        const auto call=c.stack.back();
        if((cpu.m_r[0]&~0x3fu)!=(call.saved.words[0]&~0x3fu))throw std::runtime_error("visual restored caller differs");
        record(c,call,cpu,instructions,frame,ns,"return");c.stack.pop_back();++c.completed;
    }
    const bool draw=pc==0x23a80||pc==0x240c0||pc==0x24560;
    if(!draw&&pc!=0x1f1d0&&pc!=0x1fa20&&pc!=0x1fdc0&&pc!=0x1ff60)return;
    if(c.stack.size()>=8||c.calls>=1024)throw std::runtime_error("visual call budget exceeded");
    Call call;call.id=++c.calls;call.entry=pc;call.fp=cpu.m_r[31];call.saved=saved(cpu);
    if(draw){call.pose=cpu.m_r[17];call.descriptor=cpu.m_r[16];if(!ordinary(call.pose,22)||!ordinary(call.descriptor,12))throw std::runtime_error("visual draw input outside ordinary RAM");}
    c.stack.push_back(call);record(c,call,cpu,instructions,frame,ns,"entry");
}
void end_visual_camera_capture() {
    if(!active)return;
    auto&c=*active;
    if(!c.stack.empty()||!c.calls||c.calls!=c.completed)throw std::runtime_error("incomplete visual camera calls");
    c.records.close();if(!c.records)throw std::runtime_error("cannot close visual camera records");
    std::filesystem::rename(c.directory/"camera.jsonl.tmp",c.directory/"camera.jsonl");
    std::ofstream coverage(c.directory/"coverage.json");coverage<<'[';bool first=true;
    for(auto [pc,count]:c.coverage){if(!first)coverage<<',';first=false;coverage<<"{\"pc\":"<<pc<<",\"count\":"<<count<<'}';}coverage<<"]\n";coverage.close();
    if(!coverage)throw std::runtime_error("cannot close visual coverage");
    std::ofstream marker(c.directory/"capture.json");marker<<"{\"schema\":1,\"complete\":true,\"first_frame\":"<<c.first<<",\"last_frame\":"<<c.last<<",\"calls\":"<<c.calls<<",\"completions\":"<<c.completed<<",\"events\":"<<c.events<<"}\n";marker.close();
    if(!marker)throw std::runtime_error("cannot close visual marker");active.reset();
}
}
