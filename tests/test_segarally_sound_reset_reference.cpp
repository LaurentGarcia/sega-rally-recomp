// Read-only real-program prefix. Independent MAME CPU, shared board models,
// instruction-start MMIO; no sound-on recording or physical bus-phase claim.
#include "model2a_sound_reference.h"
#include "runtime/snd_gen_support.h"
#include "segarally-sound-reset-inputs.h"
#include <bit>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

// Compile the unchanged audited source under a local namespace so this test
// can observe the native board's bus without a runtime or generated-file edit.
// Instruction bodies, guards, dispatch and scheduler calls remain untouched.
namespace observed_sound_native {using Env=sndgen::Env;}
#define sndgen observed_sound_native
#include SOUND_REFERENCE_NATIVE_SOURCE
#undef sndgen

namespace {
struct Transfer {uint32_t address;uint16_t value,mask;bool write;uint64_t board_ns,phase_cycle;};
class NativeTap final:public snd::AddressSpace {
public:
    explicit NativeTap(rt::Model2AClock &clock):clock_(clock){}
    void attach(snd::Cpu68k &cpu) {
        if(!target_) {target_=cpu.bus;cpu_=&cpu;cpu.bus=this;}
        if(cpu.bus!=this || cpu_!=&cpu) throw rt::Fatal("native tap ownership changed");
        callback_ns_=clock_.now_ns();
    }
    uint16_t read16(uint32_t address,uint16_t mask)override {
        const auto value=target_->read16(address,mask);
        if(cpu_->timing_active()) record({address,value,mask,false,clock_.now_ns(),cpu_->cycles});
        else {
            if(address<0x600000 || address>=0x680000) throw rt::Fatal("native guard outside immutable ROM profile");
            ++guard_reads;
        }
        return value;
    }
    void write16(uint32_t address,uint16_t value,uint16_t mask)override {
        if(!cpu_->timing_active()) throw rt::Fatal("native write outside instruction start");
        record({address,uint16_t(value&mask),mask,true,clock_.now_ns(),cpu_->cycles});target_->write16(address,value,mask);
    }
    std::vector<Transfer> rows;
    uint64_t guard_reads=0;
private:
    rt::Model2AClock &clock_;
    snd::AddressSpace *target_=nullptr;
    snd::Cpu68k *cpu_=nullptr;
    uint64_t callback_ns_=0;
    void record(Transfer row) {
        if(clock_.now_ns()!=callback_ns_) throw rt::Fatal("native transfer advanced time inside instruction");
        if(rows.size()==128) throw rt::Fatal("native callback evidence bound");
        rows.push_back(row);
    }
};
NativeTap *tap=nullptr;
uint64_t checks=0,now=0;
struct Witness {uint64_t count=0,cycles=0;uint32_t pc=0;} native_witness,reference_witness;
void require(bool value,const char *message) {
    ++checks;if(value)return;
    std::fprintf(stderr,"first divergence native N%llu C%llu PC%06x ref N%llu C%llu PC%06x\n",
        (unsigned long long)native_witness.count,(unsigned long long)native_witness.cycles,native_witness.pc,
        (unsigned long long)reference_witness.count,(unsigned long long)reference_witness.cycles,reference_witness.pc);
    throw rt::Fatal(std::string(message)+" at board_ns="+std::to_string(now));
}
std::vector<uint8_t> load(const char *path,size_t size) {
    require(std::filesystem::file_size(path)==size,"bound actual image size");std::ifstream file(path,std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};require(bytes.size()==size,"actual image read");return bytes;
}
}
namespace sndgen {
bool has_code(uint32_t pc){return observed_sound_native::has_code(pc);}
uint64_t native_instructions(){return observed_sound_native::native_instructions();}
void run(Env &env){if(!tap)throw rt::Fatal("native tap unbound");tap->attach(env.c);observed_sound_native::run(env);}
}

int main(int argc,char **argv) {
    try {
        require(argc==4,"actual program/samples and exclusive state path required");
        require(!std::filesystem::exists(argv[3]),"refuse existing state file");
        snd::Model2ASound::Images image{load(argv[1],0x80000),load(argv[2],0x800000)};
        rt::Model2AClock nc,rc;snd::Model2Scsp::ReferenceRandom nr,rr;NativeTap observer(nc);tap=&observer;
        snd::Model2ASound native(nc,image,[&]{return nr();},256);
        sound_reference::OracleBoard reference(rc,image,[&]{return rr();},256);
        auto *nb=dynamic_cast<const snd::Model2ASoundBus *>(native.cpu().bus);require(nb!=nullptr,"native actual board bus");
        require(nb->sound_ram().data()!=reference.bus().sound_ram().data(),"separate board RAM ownership");
        const auto program=image.program;uint64_t boundaries=0,sample_frames=0,data_rows=0,prefetch_rows=0;bool before_seen=false,first_write=false;
        uint64_t before_cycles=0,before_ns=0;Transfer first_native{};sound_reference::OracleBoard::Access first_reference{};
        const auto compare=[&](bool reset) {
            const auto n=native.stats();const auto r=reference.stats();const auto &a=native.cpu();const auto &b=reference.cpu();
            native_witness={n.instructions,n.cycles,n.pc};reference_witness={r.instructions,r.cycles,r.pc};
            if(n.instructions!=r.instructions || n.cycles!=r.cycles || n.pc!=r.pc)
                std::fprintf(stderr,"CPU boundary native N%llu C%llu PC%06x ref N%llu C%llu PC%06x\n",(unsigned long long)n.instructions,(unsigned long long)n.cycles,n.pc,(unsigned long long)r.instructions,(unsigned long long)r.cycles,r.pc);
            require(n.status==snd::Model2ASound::Status::Running,"native running");
            require(n.instructions==r.instructions && n.cycles==r.cycles && n.pc==r.pc && n.previous_dispatch==r.previous_dispatch,"CPU instructions/cycles/PC/dispatch");
            require(n.irq_entries==0 && r.irq_entries==0 && a.irq_line==0 && reference.irq_line()==0 && !a.nmi_pending && !reference.nmi_pending(),"prefix has no IRQ activity");
            require(b.m_sp==16,"supervisor-only prefix");
            for(unsigned i=0;i<8;++i) {require(a.d[i]==b.m_da[i],"D register");require(a.a[i]==b.m_da[i==7?16:i+8],"A register");}
            require(a.other_sp==b.m_da[15] && a.sr()==b.m_sr,"full SR and inactive stack bank");
            require(std::equal(nb->sound_ram().begin(),nb->sound_ram().end(),reference.bus().sound_ram().begin()),"full separate512KiB RAM");
            require(nc.snapshot()==rc.snapshot(),"queue time/deadlines/insertion order");require(nr.state==rr.state,"separate RNG state");
            require(n.sample_frames==reference.sample_count() && n.dropped_samples==0 && reference.dropped_samples()==0,"sample count/no drops");
            const auto ns=native.take_samples(),rs=reference.take_samples();require(ns.size()==rs.size(),"sample drain lengths");
            for(size_t i=0;i<ns.size();++i) require(std::isfinite(ns[i].left) && std::isfinite(ns[i].right) &&
                std::bit_cast<uint32_t>(ns[i].left)==std::bit_cast<uint32_t>(rs[i].left) && std::bit_cast<uint32_t>(ns[i].right)==std::bit_cast<uint32_t>(rs[i].right),"raw float PCM bits");
            sample_frames+=ns.size();
            std::vector<Transfer> expected;
            for(const auto &e:reference.accesses()) {
                if(e.kind=='i')throw rt::Fatal("unexpected prefix CPU-space access");
                if(e.kind=='p') {
                    ++prefetch_rows;
                    require(!e.write && e.mask==0xffff,"plain opcode prefetch");
                    if(reset && e.address<16) require(e.data==uint16_t(program[e.address]<<8|program[e.address+1]),"reset vector prefetch bytes");
                    else {require(e.address>=0x600000 && e.address+1<0x680000,"prefetch restricted to actual immutable ROM");const auto offset=e.address-0x600000;require(e.data==uint16_t(program[offset]<<8|program[offset+1]),"actual persistent prefetched word");}
                } else {
                    expected.push_back({e.address,e.data,e.mask,e.write,e.board_ns,e.phase_cycle});
                    if(e.address>=0x100000 && e.address<0x101000) {
                        require(!first_write && e.write && e.address==0x100400 && e.data==0x0300 && e.mask==0xff00,"exact first reference SCSP write");
                        first_reference=e;
                    }
                }
            }
            if(!reset) {
                require(observer.rows.size()==expected.size(),"ordered data transfer count");
                for(size_t i=0;i<expected.size();++i) {
                    const auto &x=observer.rows[i],&y=expected[i];
                    const bool same=x.address==y.address && x.value==y.value && x.mask==y.mask && x.write==y.write && x.board_ns==y.board_ns;
                    if(!same)std::fprintf(stderr,"transfer%zu native %06x/%04x/%04x write%d ns%llu phase%llu ref %06x/%04x/%04x write%d ns%llu phase%llu\n",i,x.address,x.value,x.mask,int(x.write),(unsigned long long)x.board_ns,(unsigned long long)x.phase_cycle,y.address,y.value,y.mask,int(y.write),(unsigned long long)y.board_ns,(unsigned long long)y.phase_cycle);
                    require(same,"ordered data read/write address/value/mask at instruction start");
                    // Phase cycles are retained separately, not normalized or
                    // asserted equal: physical per-bus scheduling is unproved.
                    if(x.address>=0x100000 && x.address<0x101000) {
                        require(before_seen && !first_write && x.write && x.address==0x100400 && x.value==0x0300 && x.mask==0xff00,"exact first native SCSP write");first_native=x;first_write=true;
                    }
                }
            }
            data_rows+=expected.size();observer.rows.clear();reference.drain_accesses();++boundaries;
            require(boundaries<=1000000 && n.instructions<=278302 && now<=2000000000,"finite instruction/event/virtual time caps");
            if(n.instructions && n.instructions%10000==0 && n.previous_dispatch!=0) {
                static uint64_t last=0;if(n.instructions!=last){last=n.instructions;std::printf("progress instructions=%llu cycles=%llu PC=%06x board_ns=%llu\n",(unsigned long long)n.instructions,(unsigned long long)n.cycles,n.pc,(unsigned long long)now);std::fflush(stdout);}
            }
        };
        // General registers start zero by harness agreement, not hardware
        // reset proof; the real startup explicitly initializes its used state.
        compare(true);
        while(!first_write) {
            require(nc.next_deadline() && nc.next_deadline()==rc.next_deadline(),"next paired deadline");
            const auto n=native.stats();
            if(n.pc==0x600184) {
                require(n.instructions==278301,"exact completed count before first SCSP write");
                require(std::all_of(nb->sound_ram().begin(),nb->sound_ram().end(),[](auto v){return v==0;}),"real startup cleared entire RAM");
                before_seen=true;before_cycles=n.cycles;before_ns=*nc.next_deadline();
            }
            now=*nc.next_deadline();require(now<=2000000000,"next event virtual-time bound");nc.advance(now);rc.advance(now);compare(false);
        }
        const auto n=native.stats();require(n.instructions==278302 && n.pc==0x60018a,"stop exactly after first actual SCSP write");
        require(first_native.board_ns==before_ns && first_reference.board_ns==before_ns,"first write occurs at observed callback start");
        FILE *file=std::fopen(argv[3],"wx");require(file!=nullptr,"exclusive state output");
        std::fprintf(file,"{\"status\":\"PASS\",\"before\":{\"instructions\":278301,\"pc\":\"600184\",\"cycles\":%llu},\"after\":{\"instructions\":278302,\"pc\":\"60018a\",\"cycles\":%llu},\"first_scsp\":{\"address\":\"100400\",\"data\":\"0300\",\"mask\":\"ff00\",\"board_ns\":%llu,\"native_phase_cycle\":%llu,\"reference_phase_cycle\":%llu},\"boundaries\":%llu,\"data_rows\":%llu,\"reference_prefetch_rows\":%llu,\"native_guard_reads\":%llu,\"PCM_frames\":%llu,\"checks\":%llu,\"scope\":\"Independent CPU, shared board models, instruction-start MMIO; no physical bus-phase or sound-on recording capability\"}\n",
            (unsigned long long)before_cycles,(unsigned long long)n.cycles,(unsigned long long)before_ns,(unsigned long long)first_native.phase_cycle,(unsigned long long)first_reference.phase_cycle,
            (unsigned long long)boundaries,(unsigned long long)data_rows,(unsigned long long)prefetch_rows,(unsigned long long)observer.guard_reads,(unsigned long long)sample_frames,(unsigned long long)checks);
        require(std::fclose(file)==0,"state output finalized");
        std::printf("PASS actual reset prefix instructions=278302 cycles=%llu PC=60018a firstSCSP=100400/0300/ff00 boundaries=%llu PCMframes=%llu checks=%llu\n",(unsigned long long)n.cycles,(unsigned long long)boundaries,(unsigned long long)sample_frames,(unsigned long long)checks);return 0;
    }catch(const std::exception &e){std::fprintf(stderr,"actual sound reset FAIL: %s\n",e.what());return 1;}
}
