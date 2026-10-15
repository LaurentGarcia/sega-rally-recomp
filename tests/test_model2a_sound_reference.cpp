// Synthetic board execution, not an actual sound-ROM/recording capability.
// MAME CPU execution is independent; SCSP/UART/memory/queue implementations
// are shared models on separate instances under instruction-start MMIO.
#include "model2a_sound_reference.h"
#include "runtime/model2a_uart.h"
#include <bit>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <tuple>

namespace {
unsigned checks=0;
uint64_t current_ns=0;
void require(bool value,const char *message) {
    ++checks;
    if(!value) throw rt::Fatal(std::string(message)+" at board_ns="+std::to_string(current_ns));
}
snd::Model2ASound::Images images(const char *path) {
    std::ifstream f(path,std::ios::binary);
    snd::Model2ASound::Images result;
    result.program={std::istreambuf_iterator<char>(f),{}};result.samples.resize(0x800000);
    require(result.program.size()==0x80000,"exact synthetic program image");return result;
}
struct Link {
    rt::Model2AClock &clock;
    rt::Model2UART uart;
    rt::Model2AUART board{uart};
    rt::Model2AClock::Token next{};
    bool level=false;
    std::vector<std::tuple<uint64_t,char,bool>> trace;
    std::vector<uint8_t> received;
    unsigned ready_edges=0,physical_edges=0;
    template<class Sound> Link(rt::Model2AClock &c,Sound &sound):clock(c) {
        rt::Model2UART::Callbacks callbacks;
        callbacks.txd=[this,&sound](bool value) {trace.emplace_back(clock.now_ns(),'t',value);sound.midi_in(value);};
        callbacks.rx_ready=[this](bool value) {trace.emplace_back(clock.now_ns(),'r',value);if(value)++ready_edges;};
        uart.set_callbacks(callbacks);
        sound.on_midi_tx=[this](bool value) {trace.emplace_back(clock.now_ns(),'s',value);uart.write_rxd(value);};
        board.reset();sound.midi_in(uart.txd_level());
        for(uint8_t value:{0,0,0,0x40,0x4e,0x37}) uart.control_write(value);
        schedule();
    }
    ~Link(){clock.cancel(next);}
    void schedule() {
        next=clock.schedule_after(1000,[this](uint64_t) {
            level=!level;++physical_edges;board.clock_edge(level);schedule();
        });
    }
    void send(uint8_t byte) {require(uart.tx_ready(),"physical UART ready to transmit");uart.data_write(byte);}
    void receive(uint8_t byte) {
        require(uart.rx_ready(),"CPU echo physically reached main UART");
        const auto result=uart.data_read();require(result==byte,"physical echo data byte");received.push_back(result);
        require(!(uart.status_read()&(rt::Model2UART::ParityError|rt::Model2UART::OverrunError|rt::Model2UART::FramingError)),"physical UART framing/error status");
    }
};
void run_case(const snd::Model2ASound::Images &image,bool send_before_cpu) {
    current_ns=0;
    rt::Model2AClock native_clock,reference_clock;
    snd::Model2Scsp::ReferenceRandom native_random,reference_random;
    const uint64_t first=sound_reference::time_ns(38); // Reset: four RAM waits.
    std::function<void()> native_send,reference_send;
    const auto install_send=[&] {
        native_clock.schedule_at(first,[&](uint64_t){native_send();});
        reference_clock.schedule_at(first,[&](uint64_t){reference_send();});
    };
    if(send_before_cpu) install_send();
    snd::Model2ASound native(native_clock,image,[&]{return native_random();},256);
    sound_reference::OracleBoard reference(reference_clock,image,[&]{return reference_random();},256);
    Link native_link(native_clock,native),reference_link(reference_clock,reference);
    native_send=[&] {require(native.stats().instructions==unsigned(!send_before_cpu),"native equal-deadline insertion order");native_link.send(0xa5);};
    reference_send=[&] {require(reference.stats().instructions==unsigned(!send_before_cpu),"reference equal-deadline insertion order");reference_link.send(0xa5);};
    if(!send_before_cpu) install_send();
    native_clock.schedule_at(700000,[&](uint64_t){native_link.receive(0xa5);});
    reference_clock.schedule_at(700000,[&](uint64_t){reference_link.receive(0xa5);});
    native_clock.schedule_at(800000,[&](uint64_t){native_link.send(0x5a);});
    reference_clock.schedule_at(800000,[&](uint64_t){reference_link.send(0x5a);});
    native_clock.schedule_at(1500000,[&](uint64_t){native_link.receive(0x5a);});
    reference_clock.schedule_at(1500000,[&](uint64_t){reference_link.receive(0x5a);});
    const auto *native_bus=dynamic_cast<const snd::Model2ASoundBus *>(native.cpu().bus);
    require(native_bus!=nullptr,"native board retains its actual bus");
    uint64_t boundaries=0,pcm_frames=0,nonzero=0,irq_only=0;
    const auto compare=[&] {
        const auto n=native.stats();const auto r=reference.stats();const auto &a=native.cpu();const auto &b=reference.cpu();
        if(n.instructions!=r.instructions || n.cycles!=r.cycles || n.pc!=r.pc || n.irq_entries!=r.irq_entries)
            std::fprintf(stderr,"boundary native N%llu C%llu PC%06x IRQ%llu reference N%llu C%llu PC%06x IRQ%llu\n",
                (unsigned long long)n.instructions,(unsigned long long)n.cycles,n.pc,(unsigned long long)n.irq_entries,
                (unsigned long long)r.instructions,(unsigned long long)r.cycles,r.pc,(unsigned long long)r.irq_entries);
        require(n.status==snd::Model2ASound::Status::Running,"native remains running");
        require(n.instructions==r.instructions && n.cycles==r.cycles && n.pc==r.pc && n.previous_dispatch==r.previous_dispatch && n.irq_entries==r.irq_entries,"independent CPU instruction/IRQ/cycle boundary");
        require(b.m_sp==15 || b.m_sp==16,"reference active stack bank");
        for(unsigned i=0;i<8;++i) {
            require(a.d[i]==b.m_da[i],"independent D register");require(a.a[i]==b.m_da[i==7?b.m_sp:i+8],"independent A register");
        }
        require(a.other_sp==b.m_da[b.m_sp==16?15:16] && a.sr()==b.m_sr,"independent stack bank/full materialized SR");
        require(a.irq_line==reference.irq_line() && a.nmi_pending==reference.nmi_pending(),"pending physical IRQ state");
        require(std::equal(native_bus->sound_ram().begin(),native_bus->sound_ram().end(),reference.bus().sound_ram().begin()),"entire separate 512KiB board RAM");
        require(native_link.uart.snapshot()==reference_link.uart.snapshot() && native_link.trace==reference_link.trace,"physical UART snapshots and timestamped pin edges");
        require(native_random.state==reference_random.state,"separate RNG states");
        require(n.sample_frames==reference.sample_count() && n.dropped_samples==reference.dropped_samples() && n.dropped_samples==0,"sample counts and no buffer drops");
        require(native_clock.snapshot()==reference_clock.snapshot(),"separate queue deadlines/insertion order");
        const auto ns=native.take_samples(),rs=reference.take_samples();require(ns.size()==rs.size(),"matching sample drain size");
        for(size_t i=0;i<ns.size();++i) {
            require(std::isfinite(ns[i].left) && std::isfinite(ns[i].right),"finite native PCM");
            require(std::bit_cast<uint32_t>(ns[i].left)==std::bit_cast<uint32_t>(rs[i].left) && std::bit_cast<uint32_t>(ns[i].right)==std::bit_cast<uint32_t>(rs[i].right),"raw float PCM bits per sample drain");
            nonzero+=ns[i].left!=0 || ns[i].right!=0;
        }
        pcm_frames+=ns.size();++boundaries;
    };
    // General registers begin zero in both harness objects by agreement. This
    // initial comparison is not a claim that hardware reset defines their
    // values; the program explicitly initializes every register it depends on.
    compare();
    while(native_clock.next_deadline() && *native_clock.next_deadline()<=2000000) {
        require(native_clock.next_deadline()==reference_clock.next_deadline(),"next separate queue deadline");
        current_ns=*native_clock.next_deadline();const auto before=native.stats();
        native_clock.advance(current_ns);reference_clock.advance(current_ns);compare();
        const auto after=native.stats();
        if(after.irq_entries!=before.irq_entries) {require(after.instructions==before.instructions,"IRQ callback executes no instruction");++irq_only;}
        require(boundaries<20000,"bounded synthetic execution");
    }
    current_ns=2000000;native_clock.advance(current_ns);reference_clock.advance(current_ns);compare();
    require(native.cpu().d[1]==2 && (native.cpu().d[0]&255)==0x5a,"two real IRQ handlers/RTEs completed");
    require(native.stats().irq_entries==2 && irq_only==2,"two independently scheduled IRQ-only callbacks");
    require(native_link.received==std::vector<uint8_t>({0xa5,0x5a}) && native_link.received==reference_link.received,"physical receive/echo sequence");
    require(native_link.ready_edges==2 && native_link.physical_edges==2000,"real UART edge count");
    require(pcm_frames==89 && nonzero>0 && native.take_samples().empty() && reference.take_samples().empty(),"nonzero PCM and exact once-per-boundary drains");
    require(std::any_of(reference.accesses().begin(),reference.accesses().end(),[](auto e){return e.kind=='i';}),"MAME independently executed CPU-space IACK");
    for(const auto &access:reference.accesses()) if(access.kind=='i') require(access.data==25 && access.mask==0xffff,"recorded actual IPL1 autovector transfer");
    require(std::any_of(reference.accesses().begin(),reference.accesses().end(),[](auto e){return e.address==0x100404 && !e.write;}),"MAME read real SCSP receive register");
    std::printf("case insertion=%s instructions=%llu cycles=%llu IRQ=%llu boundaries=%llu PCMframes=%llu nonzero_frames=%llu UARTphysicaledges=%u checks=%u\n",send_before_cpu?"before_CPU":"after_CPU",
        (unsigned long long)native.stats().instructions,(unsigned long long)native.stats().cycles,(unsigned long long)native.stats().irq_entries,
        (unsigned long long)boundaries,(unsigned long long)pcm_frames,(unsigned long long)nonzero,native_link.physical_edges,checks);
}
}
int main(int argc,char **argv) {
    try {require(argc==2,"synthetic program path required");const auto image=images(argv[1]);run_case(image,true);run_case(image,false);
        std::printf("PASS %u checks; independent continuous MAME M68000, separate shared-model boards, instruction-start MMIO only; no sound-on recording/SCSP-independent/physical bus-phase/hardware claim\n",checks);return 0;
    } catch(const std::exception &error) {std::fprintf(stderr,"sound board reference FAIL: %s\n",error.what());return 1;}
}
