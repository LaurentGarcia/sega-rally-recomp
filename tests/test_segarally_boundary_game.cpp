// Actual generated Sega Rally, two Model2AMachines in one compatible binary.
// Differential scheduling evidence only: devices/generated CPU are shared code.
#include "runtime/model2a_machine.h"
#include "runtime/video.h"
#include "tools/srally_boot/input_replay.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

#ifndef M2_INSTRUCTION_BOUNDARY_HOOK
#error Regenerate every native game chunk with the exact boundary ABI.
#endif
namespace {
using Machine=rt::Model2AMachine;
struct Write {
    uint64_t instructions,cycles;
    uint32_t pc,address,value;
    unsigned width;
    bool operator==(const Write&)const=default;
};
struct Irq {
    uint64_t instructions,cycles;
    int vector;uint32_t pc;bool pending;
    bool operator==(const Irq&)const=default;
};
struct Recorder final:rt::Bus {
    Machine &machine;rt::Cpu &cpu;rt::Bus *original;
    decltype(rt::Cpu::on_take) original_irq;void *original_context;
    std::vector<Write> writes;std::vector<Irq> interrupts;
    uint64_t write_total=0,irq_total=0;
    explicit Recorder(Machine&m):machine(m),cpu(const_cast<rt::Cpu&>(m.cpu())),original(cpu.bus),
        original_irq(cpu.on_take),original_context(cpu.on_take_ctx){
        cpu.bus=this;cpu.on_take=on_irq;cpu.on_take_ctx=this;
    }
    ~Recorder()override{
        // Restore before Machine destruction, including exceptional exits.
        cpu.bus=original;cpu.on_take=original_irq;cpu.on_take_ctx=original_context;
    }
    static void on_irq(void*context,int vector,uint32_t pc,bool pending){
        auto&r=*static_cast<Recorder*>(context);
        r.interrupts.push_back({r.machine.stats().instructions,r.cpu.reference_cycles,vector,pc,pending});++r.irq_total;
        if(r.original_irq)r.original_irq(r.original_context,vector,pc,pending);
    }
    void record(uint32_t a,uint32_t v,unsigned width){
        // Capture attempted writes before forwarding; includes failed accesses.
        writes.push_back({machine.stats().instructions,cpu.reference_cycles,cpu.m_IP,a,v,width});++write_total;
    }
    uint32_t fetch(uint32_t a)override{return original->fetch(a);}
    uint8_t read_byte(uint32_t a)override{return original->read_byte(a);}
    uint16_t read_word(uint32_t a)override{return original->read_word(a);}
    uint32_t read_dword(uint32_t a)override{return original->read_dword(a);}
    void write_byte(uint32_t a,uint8_t v)override{record(a,v,1);original->write_byte(a,v);}
    void write_word(uint32_t a,uint16_t v)override{record(a,v,2);original->write_word(a,v);}
    void write_dword(uint32_t a,uint32_t v)override{record(a,v,4);original->write_dword(a,v);}
    uint16_t flags(uint32_t a)override{return original->flags(a);}
    rt::SceneryState*scenery()override{return original->scenery();}
    void clear(){writes.clear();interrupts.clear();}
};
[[noreturn]] void fail(const char*field,uint64_t at){throw std::runtime_error(std::string(field)+" differs at completed instruction "+std::to_string(at));}
std::vector<uint8_t>load(const char*path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error(std::string("cannot open ")+path);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
void compare_cpu(const rt::Cpu&a,const rt::Cpu&b,uint64_t at){
    if(std::memcmp(a.m_r,b.m_r,sizeof(a.m_r)))fail("CPU registers",at);
    if(std::memcmp(a.m_rcache,b.m_rcache,sizeof(a.m_rcache))||std::memcmp(a.m_rcache_frame_addr,b.m_rcache_frame_addr,sizeof(a.m_rcache_frame_addr)))fail("register cache",at);
    if(std::memcmp(a.m_fp,b.m_fp,sizeof(a.m_fp)))fail("FP bit patterns",at);
    if(std::memcmp(a.m_irq_line_state,b.m_irq_line_state,sizeof(a.m_irq_line_state)))fail("CPU IRQ lines",at);
    if(a.m_SAT!=b.m_SAT||a.m_PRCB!=b.m_PRCB||a.m_PC!=b.m_PC||a.m_AC!=b.m_AC||a.m_IP!=b.m_IP||a.m_PIP!=b.m_PIP||a.m_ICR!=b.m_ICR||a.m_rcache_pos!=b.m_rcache_pos||a.reference_cycles!=b.reference_cycles||a.m_immediate_irq!=b.m_immediate_irq||a.m_immediate_vector!=b.m_immediate_vector||a.m_immediate_pri!=b.m_immediate_pri)fail("CPU/control state",at);
}
void compare_stats(const Machine::Stats&a,const Machine::Stats&b){
    const auto key=[](const Machine::Stats&s){return std::tuple{
        s.instructions,s.reference_cycles,s.board_time_ns,s.frames_rendered,s.frames_parsed,s.screen_frame,s.latched_frame,
        s.tgp_instructions,s.tx_pin_changes,s.polygons,s.frames_with_geometry,s.first_geometry_frame,
        s.sound_instructions,s.sound_cycles,s.sound_irqs,s.sound_samples,s.sound_dropped_samples,
        s.sound_pc,s.sound_previous_dispatch,s.pc,s.previous_dispatch,s.irq_entries,s.tgp_booted,s.uart_mode,s.uart_command};};
    if(key(a)!=key(b))fail("Machine clock/device counters",a.instructions);
}
void compare_result(const Machine::RunResult&a,const Machine::RunResult&b){
    if(std::tie(a.reason,a.pc,a.instructions,a.message,a.address,a.value,a.width,a.write)!=
       std::tie(b.reason,b.pc,b.instructions,b.message,b.address,b.value,b.width,b.write))fail("Machine return/frontier",a.instructions);
}
uint64_t framebuffer_hash(const std::vector<uint32_t>&pixels){
    uint64_t h=1469598103934665603ULL;
    for(uint32_t p:pixels)for(unsigned shift=0;shift<32;shift+=8){h^=uint8_t(p>>shift);h*=1099511628211ULL;}
    return h;
}
void compare(Machine&a,Machine&b,Recorder&ra,Recorder&rb){
    const auto x=a.stats(),y=b.stats();compare_stats(x,y);compare_cpu(a.cpu(),b.cpu(),x.instructions);
    if(ra.writes!=rb.writes)fail("cycle/count/PC-stamped ordered CPU bus writes",x.instructions);
    if(ra.interrupts!=rb.interrupts)fail("IRQ delivery order",x.instructions);
    if(a.video().screen()!=b.video().screen())fail("complete rendered framebuffer",x.instructions);
    const auto&d=a.diagnostics();const auto&e=b.diagnostics();
    if(std::tie(d.reference_gap_reads,d.reference_gap_writes,d.reference_gap_last_write,d.irq_reset_writes)!=
       std::tie(e.reference_gap_reads,e.reference_gap_writes,e.reference_gap_last_write,e.irq_reset_writes))fail("bus diagnostics",x.instructions);
    // These host snapshots do not read guest MMIO or advance board time.
    if(a.save_eeprom()!=b.save_eeprom()||a.save_backup()!=b.save_backup())fail("EEPROM/backup bytes",x.instructions);
    ra.clear();rb.clear();
}
}
int main(int argc,char**argv){
    if(argc<8){std::cerr<<"usage: boundary_game PROGRAM MAIN_DATA TGP_TABLES COPRO_DATA POLYGONS TEXTURES INSTRUCTION_LIMIT [REPLAY] [--mixed-resume]\n";return 2;}
    try{
        size_t used=0;const std::string text=argv[7];const uint64_t limit=std::stoull(text,&used);
        if(used!=text.size()||!limit||limit>2'000'000'000)throw std::runtime_error("budget must be1..2000000000");
        bool mixed=false;std::string replay_path;
        for(int n=8;n<argc;++n){if(std::string(argv[n])=="--mixed-resume")mixed=true;else if(replay_path.empty())replay_path=argv[n];else throw std::runtime_error("unexpected argument");}
        Machine::Images images;images.program=load(argv[1]);images.main_data=load(argv[2]);images.tgp_tables=load(argv[3]);images.copro_data=load(argv[4]);images.polygons=load(argv[5]);images.textures=load(argv[6]);
        Machine::Options old_options,new_options;old_options.instruction_boundary_hook=false;new_options.instruction_boundary_hook=true;
        Machine old_machine(images,old_options),new_machine(std::move(images),new_options);
        // Destruction order restores observers before either Machine is freed.
        Recorder old_record(old_machine),new_record(new_machine);
        std::unique_ptr<srally::InputReplay>old_replay,new_replay;
        if(!replay_path.empty()){
            std::ifstream a(replay_path),b(replay_path);if(!a||!b)throw std::runtime_error("cannot open replay");
            old_replay=std::make_unique<srally::InputReplay>(a);new_replay=std::make_unique<srally::InputReplay>(b);
        }
        compare(old_machine,new_machine,old_record,new_record);
        uint64_t last_mixed_frame=UINT64_MAX,returns=0,frame_checks=0;
        while(old_machine.stats().instructions<limit){
            const auto before=old_machine.stats();
            if(old_replay){
                const bool a=old_replay->apply(before.frames_rendered,old_machine.inputs());
                const bool b=new_replay->apply(new_machine.stats().frames_rendered,new_machine.inputs());
                if(a!=b||old_replay->applied()!=new_replay->applied())fail("independent replay cursors",before.instructions);
                if(a)std::cout<<"input frame="<<before.frames_rendered<<" instructions="<<before.instructions<<" cycles="<<before.reference_cycles<<'\n';
            }
            const bool one=mixed&&before.frames_rendered%37==0&&last_mixed_frame!=before.frames_rendered;
            if(one)last_mixed_frame=before.frames_rendered;
            const uint64_t budget=one?1:limit-before.instructions;
            const auto a=one?old_machine.run_for(budget):old_machine.run_until_frame(budget);
            const auto b=one?new_machine.run_for(budget):new_machine.run_until_frame(budget);
            compare_result(a,b);compare(old_machine,new_machine,old_record,new_record);++returns;
            const auto after=old_machine.stats();
            if(after.frames_rendered!=before.frames_rendered){++frame_checks;if(after.frames_rendered%120==0)std::cout<<"frame="<<after.frames_rendered<<" instructions="<<after.instructions<<" cycles="<<after.reference_cycles<<'\n';}
            if(a.reason!=Machine::Reason::Frame&&a.reason!=Machine::Reason::Budget){
                std::cout<<"MATCHED_FRONTIER instructions="<<a.instructions<<" pc="<<std::hex<<a.pc<<std::dec<<" reason="<<int(a.reason)<<" message="<<a.message<<'\n';return 3;
            }
            if(after.instructions==before.instructions&&after.frames_rendered==before.frames_rendered)throw std::runtime_error("run made no instruction/frame progress");
        }
        const auto s=old_machine.stats();
        std::cout<<"PASS instructions="<<s.instructions<<" cycles="<<s.reference_cycles<<" frames="<<s.frames_rendered<<" frame_checks="<<frame_checks<<" returns="<<returns<<" writes="<<old_record.write_total<<" irqs="<<old_record.irq_total<<" mixed="<<mixed<<" framebuffer_fnv="<<std::hex<<framebuffer_hash(old_machine.video().screen())<<std::dec;
        if(old_replay)std::cout<<" input_applied="<<old_replay->applied()<<" input_pending="<<old_replay->size()-old_replay->applied();
        std::cout<<" private_device_queues_compared=false independent_hardware=false\n";
    }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
