// Test oracle only: native reset execution versus the inherited i960 core.
// ROM images and generated native objects are supplied from ignored build/.
#include "refcore/i960_ref.h"
#include "runtime/gen_support.h"
#include "runtime/model2a_bus.h"
#include "runtime/model2a_clock.h"
#include "tools/srally_boot/input_replay.h"
#include <cstdio>
#include <deque>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>
#ifdef M2_SRALLY_IO
#include "runtime/model2a_io.h"
#include "runtime/model2_eeprom.h"
#endif
#ifdef M2_SRALLY_CONTROL
#include "runtime/model2a_control.h"
#endif
#ifdef M2_SRALLY_UART
#include "runtime/model2a_uart.h"
#endif
#ifdef M2_SRALLY_VIDEO
#include "runtime/model2a_video.h"
#include "runtime/geo.h"
#endif
#ifdef M2_SRALLY_COMM
#include "runtime/model2a_comm.h"
#endif
#ifdef M2_SRALLY_TGP
#include "runtime/m2_tgp_board.h"
#include <memory>
struct NativeTgpModel : rt::TgpBoardModel {
    rt::TgpBoard& board;
    explicit NativeTgpModel(rt::TgpBoard& b):TgpBoardModel(b),board(b) {}
    Check read(uint32_t address,uint32_t mask,uint32_t& value) override {
        if(address>=0x00900000 && address<=0x0097ffff) board.sync();
        return TgpBoardModel::read(address,mask,value);
    }
};
#endif

struct ObservedBus : rt::Bus {
    rt::Model2ABus board;
    const rt::Cpu* core=nullptr;
    // Capture when the write occurs, not just the completed instruction's
    // total: this checks charging order around MMIO and register-frame spills.
    std::vector<std::tuple<uint32_t,unsigned,uint32_t,uint64_t>> writes;
    ObservedBus(const std::vector<uint8_t>& p,const std::vector<uint8_t>& d):board(p,d) {}
    uint32_t fetch(uint32_t a) override { return board.fetch(a); }
    uint8_t read_byte(uint32_t a) override { return board.read_byte(a); }
    uint16_t read_word(uint32_t a) override { return board.read_word(a); }
    uint32_t read_dword(uint32_t a) override { return board.read_dword(a); }
    uint16_t flags(uint32_t a) override { return board.flags(a); }
    uint64_t cycles() const { return core?core->reference_cycles:0; }
    void write_byte(uint32_t a,uint8_t v) override { const auto at=cycles();board.write_byte(a,v);writes.emplace_back(a,1,v,at); }
    void write_word(uint32_t a,uint16_t v) override { const auto at=cycles();board.write_word(a,v);writes.emplace_back(a,2,v,at); }
    void write_dword(uint32_t a,uint32_t v) override { const auto at=cycles();board.write_dword(a,v);writes.emplace_back(a,4,v,at); }
};

static std::vector<uint8_t> load(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if(!file) throw rt::Fatal("cannot read input image");
    return {std::istreambuf_iterator<char>(file),{}};
}
static uint64_t time_ns(const rt::Cpu& cpu) {
    if(cpu.reference_cycles>UINT64_MAX/40) throw rt::Fatal("board clock overflow");
    return cpu.reference_cycles*40;
}
struct IRQTrace {
    rt::Cpu& cpu;
    void (*forward)(void*,int,uint32_t,bool);
    void* forward_context;
    uint64_t total=0;
    std::vector<std::tuple<int,uint32_t,bool,uint64_t>> events;
    explicit IRQTrace(rt::Cpu& c):cpu(c),forward(c.on_take),forward_context(c.on_take_ctx) {
        c.on_take=take;c.on_take_ctx=this;
    }
    static void take(void* context,int vector,uint32_t ip,bool pending) {
        auto& self=*static_cast<IRQTrace*>(context);
        self.events.emplace_back(vector,ip,pending,self.cpu.reference_cycles);
        self.total++;
        if(self.forward) self.forward(self.forward_context,vector,ip,pending);
    }
};
static bool same(const rt::Cpu& a,const rt::Cpu& b) {
    // PIP is the interpreter's last decoded instruction, a diagnostic field
    // that generated code does not update. It is not architectural state.
    return !std::memcmp(a.m_r,b.m_r,sizeof(a.m_r)) &&
        !std::memcmp(a.m_fp,b.m_fp,sizeof(a.m_fp)) &&
        !std::memcmp(a.m_rcache,b.m_rcache,sizeof(a.m_rcache)) &&
        !std::memcmp(a.m_rcache_frame_addr,b.m_rcache_frame_addr,sizeof(a.m_rcache_frame_addr)) &&
        !std::memcmp(a.m_irq_line_state,b.m_irq_line_state,sizeof(a.m_irq_line_state)) &&
        a.m_rcache_pos==b.m_rcache_pos && a.m_IP==b.m_IP && a.m_AC==b.m_AC &&
        a.m_PC==b.m_PC && a.m_SAT==b.m_SAT && a.m_PRCB==b.m_PRCB && a.m_ICR==b.m_ICR &&
        a.m_immediate_irq==b.m_immediate_irq && a.m_immediate_vector==b.m_immediate_vector &&
        a.m_immediate_pri==b.m_immediate_pri && a.reference_cycles==b.reference_cycles;
}
int main(int argc,char** argv) {
    bool enable_io=false,enable_comm=false,enable_control=false,enable_video=false,enable_screen=false,enable_uart=false;
    const char *replay_path=nullptr;
    int positional_argc=argc;
    while(positional_argc>1) {
        if(positional_argc>2 && std::strcmp(argv[positional_argc-2],"--input-replay")==0 && !replay_path) {
            replay_path=argv[positional_argc-1];positional_argc-=2;continue;
        }
        const char* option=argv[positional_argc-1];
        if(std::strcmp(option,"--io")==0 && !enable_io) enable_io=true;
        else if(std::strcmp(option,"--comm")==0 && !enable_comm) enable_comm=true;
        else if(std::strcmp(option,"--control")==0 && !enable_control) enable_control=true;
        else if(std::strcmp(option,"--video")==0 && !enable_video) enable_video=true;
        else if(std::strcmp(option,"--screen")==0 && !enable_screen) enable_screen=true;
        else if(std::strcmp(option,"--uart")==0 && !enable_uart) enable_uart=true;
        else break;
        positional_argc--;
    }
    if(positional_argc!=4 && positional_argc!=6 && positional_argc!=8) return 2;
    if(enable_video!=(positional_argc==8)) return 2;
    if(enable_screen && (!enable_control || !enable_video)) return 2;
    if(enable_uart && !enable_control) return 2;
    if(replay_path && (!enable_screen || !enable_io)) return 2;
    try {
        std::optional<srally::InputReplay> native_replay,reference_replay;
        if(replay_path) {
            std::ifstream stream(replay_path);
            if(!stream) throw rt::Fatal("cannot read input replay");
            native_replay.emplace(stream);
            reference_replay=native_replay;
        }
        auto p=load(argv[1]),d=load(argv[2]);
        ObservedBus nb(p,d),rb(p,d);
        rt::Cpu native(&nb);rt::I960Ref ref(&rb);
        nb.core=&native;rb.core=&ref;
        rt::Lockstep ls(native);
        IRQTrace native_irqs(native),reference_irqs(ref);
        rt::Model2AClock native_clock,reference_clock;
        bool native_syncing=false,reference_syncing=false;
        const auto sync_native=[&] {
            if(native_syncing) return;
            native_syncing=true;
            try { native_clock.advance(time_ns(native));native_syncing=false; }
            catch(...) { native_syncing=false;throw; }
        };
        const auto sync_reference=[&] {
            if(reference_syncing) return;
            reference_syncing=true;
            try { reference_clock.advance(time_ns(ref));reference_syncing=false; }
            catch(...) { reference_syncing=false;throw; }
        };
        nb.board.set_before_access(sync_native);rb.board.set_before_access(sync_reference);
        std::deque<std::pair<unsigned,bool>> native_edges,reference_edges;
        std::unique_ptr<rt::Model2AScreen> native_screen,reference_screen;
        if(enable_screen) {
            native_screen=std::make_unique<rt::Model2AScreen>(native_clock);
            reference_screen=std::make_unique<rt::Model2AScreen>(reference_clock);
        }
        const auto same_clock=[&] {
            return native_clock.snapshot()==reference_clock.snapshot() && native_edges==reference_edges &&
                (!enable_screen || (native_screen->screen_frame()==reference_screen->screen_frame() &&
                                    native_screen->latched_frame()==reference_screen->latched_frame()));
        };
        std::vector<std::tuple<bool,uint64_t,uint64_t>> native_frames,reference_frames;
        uint64_t native_parses=0,reference_parses=0,native_renders=0,reference_renders=0;
        uint64_t native_screen_hash=0,reference_screen_hash=0;
        const auto report_screen=[&] {
            if(enable_screen) std::printf("screen frames=%llu latched=%llu parses=%llu renders=%llu irq_entries=%llu pixel_hash=%016llx\n",
                (unsigned long long)native_screen->screen_frame(),(unsigned long long)native_screen->latched_frame(),
                (unsigned long long)native_parses,(unsigned long long)native_renders,(unsigned long long)native_irqs.total,
                (unsigned long long)native_screen_hash);
        };
#ifdef M2_SRALLY_CONTROL
        rt::Model2AControl native_control(native_clock);
        rt::Model2AControl reference_control(reference_clock);
        if(enable_control) {
            nb.board.add_model(&native_control);rb.board.add_model(&reference_control);
            native_control.on_irq_line=[&](unsigned line,bool asserted) {
                native_edges.emplace_back(line,asserted);
            };
            reference_control.on_irq_line=[&](unsigned line,bool asserted) {
                reference_edges.emplace_back(line,asserted);
            };
            std::printf("Control parity independent_devices=true timers_connected=true vblank_connected=%s uart_connected=%s\n",
                enable_screen?"true":"false",enable_uart?"true":"false");
        }
        const auto advance_control=[&] {
            if(enable_control) {
                while(!native_edges.empty()) {
                    const auto [line,asserted]=native_edges.front();native_edges.pop_front();
                    native.execute_set_input(int(line),asserted?1:0);
                }
                while(!reference_edges.empty()) {
                    const auto [line,asserted]=reference_edges.front();reference_edges.pop_front();
                    ref.execute_set_input(int(line),asserted?1:0);
                }
                native.check_immediate_irqs();ref.check_immediate_irqs();
            }
        };
        const auto same_control=[&] {
            return !enable_control || native_control.snapshot()==reference_control.snapshot();
        };
#else
        if(enable_control) throw rt::Fatal("control support not linked into test");
        const auto advance_control=[] {};
        const auto same_control=[] { return true; };
#endif
#if defined(M2_SRALLY_UART) && defined(M2_SRALLY_CONTROL)
        rt::Model2UART native_uart_chip,reference_uart_chip;
        rt::Model2AUART native_uart(native_uart_chip),reference_uart(reference_uart_chip);
        bool native_uart_clock=false,reference_uart_clock=false;
        std::vector<std::tuple<unsigned,bool,uint64_t>> native_uart_events,reference_uart_events;
        uint64_t native_uart_edges=0,reference_uart_edges=0;
        if(enable_uart) {
            const auto callbacks=[](rt::Model2UART& chip,rt::Model2AControl& control,
                                    rt::Model2AClock& clock,auto& events) {
                rt::Model2UART::Callbacks cb;
                cb.txd=[&events,&clock](bool level) { events.emplace_back(0,level,clock.now_ns()); };
                cb.dtr=[&events,&clock](bool level) { events.emplace_back(1,level,clock.now_ns()); };
                cb.rts=[&events,&clock](bool level) { events.emplace_back(2,level,clock.now_ns()); };
                cb.rx_ready=[&events,&clock,&chip,&control](bool level) {
                    events.emplace_back(3,level,clock.now_ns());control.sound_ready(chip.tx_ready(),chip.rx_ready());
                };
                cb.tx_ready=[&events,&clock,&chip,&control](bool level) {
                    events.emplace_back(4,level,clock.now_ns());control.sound_ready(chip.tx_ready(),chip.rx_ready());
                };
                cb.tx_empty=[&events,&clock](bool level) { events.emplace_back(5,level,clock.now_ns()); };
                cb.syndet=[&events,&clock](bool level) { events.emplace_back(6,level,clock.now_ns()); };
                return cb;
            };
            native_uart_chip.set_callbacks(callbacks(native_uart_chip,native_control,native_clock,native_uart_events));
            reference_uart_chip.set_callbacks(callbacks(reference_uart_chip,reference_control,reference_clock,reference_uart_events));
            native_uart.reset();reference_uart.reset();
            nb.board.add_model(&native_uart);rb.board.add_model(&reference_uart);
        }
        std::function<void(uint64_t)> native_uart_tick,reference_uart_tick;
        if(enable_uart) {
            native_uart_tick=[&](uint64_t deadline) {
                native_uart_clock=!native_uart_clock;native_uart_edges++;
                native_uart_events.emplace_back(7,native_uart_clock,deadline);
                native_uart.clock_edge(native_uart_clock);
                native_clock.schedule_after(1000,native_uart_tick);
            };
            reference_uart_tick=[&](uint64_t deadline) {
                reference_uart_clock=!reference_uart_clock;reference_uart_edges++;
                reference_uart_events.emplace_back(7,reference_uart_clock,deadline);
                reference_uart.clock_edge(reference_uart_clock);
                reference_clock.schedule_after(1000,reference_uart_tick);
            };
            // Pinned clock starts low; high1us, low2us; TXC then RXC.
            native_clock.schedule_after(1000,native_uart_tick);
            reference_clock.schedule_after(1000,reference_uart_tick);
            std::puts("UART parity independent_devices=true edge_ns=1000 rx_idle=true scsp_connected=false");
        }
        const auto clear_uart_events=[&] { native_uart_events.clear();reference_uart_events.clear(); };
        const auto same_uart=[&] {
            return !enable_uart || (native_uart_chip.snapshot()==reference_uart_chip.snapshot() &&
                native_uart_clock==reference_uart_clock && native_uart_edges==reference_uart_edges &&
                native_uart_events==reference_uart_events);
        };
        const auto report_uart=[&] {
            if(enable_uart) std::printf("uart edges=%llu mode=%02x command=%02x tx_ready=%d rx_ready=%d\n",
                (unsigned long long)native_uart_edges,unsigned(native_uart_chip.mode()),unsigned(native_uart_chip.command()),
                native_uart_chip.tx_ready(),native_uart_chip.rx_ready());
        };
#else
        if(enable_uart) throw rt::Fatal("UART requires linked control and UART support");
        const auto clear_uart_events=[] {};
        const auto same_uart=[] { return true; };
        const auto report_uart=[] {};
#endif
#ifdef M2_SRALLY_COMM
        rt::Model2AComm native_comm,reference_comm;
        if(enable_comm) {
            nb.board.add_model(&native_comm);rb.board.add_model(&reference_comm);
            std::printf("Communication parity independent_devices=true peer_connected=false firmware_executed=false vblank_connected=%s\n",enable_screen?"true":"false");
        }
        const auto same_comm=[&] {
            if(!enable_comm) return true;
            return native_comm.link()==reference_comm.link() &&
                !std::memcmp(native_comm.region(0x01a00000),reference_comm.region(0x01a00000),0x4000);
        };
#else
        if(enable_comm) throw rt::Fatal("communication support not linked into test");
        const auto same_comm=[] { return true; };
#endif
#ifdef M2_SRALLY_IO
        // Each independent board observes its own engine's reference clock.
        // 25 MHz -> 40 ns/cycle. This is pinned emulator timing, not PCB proof.
        rt::Model2Eeprom native_eeprom,reference_eeprom;
        native_eeprom.reset();reference_eeprom.reset();
        rt::Model2AIO native_io(native_eeprom,[&] { return native_clock.now_ns(); });
        rt::Model2AIO reference_io(reference_eeprom,[&] { return reference_clock.now_ns(); });
        if(enable_io) {
            nb.board.add_model(&native_io);rb.board.add_model(&reference_io);
            std::puts("I/O parity reference_cycle_clock=true ns_per_cycle=40 independent_devices=true pcb_timing_verified=false");
        }
        const auto replay_inputs=[&] {
            if(!native_replay) return;
            const bool applied=native_replay->apply(native_renders,native_io.inputs);
            if(applied!=reference_replay->apply(reference_renders,reference_io.inputs))
                throw rt::Fatal("input replay boundary mismatch");
            if(applied) std::printf("input_event completed_frames=%llu instructions=%llu cycles=%llu in0=%02x\n",
                (unsigned long long)native_renders,(unsigned long long)ls.count,
                (unsigned long long)native.reference_cycles,unsigned(native_io.inputs.in0));
        };
        const auto report_inputs=[&] {
            if(native_replay) std::printf("input_replay events=%zu applied=%zu pending=%zu frame_origin=completed_renders independent_cursors=true\n",
                native_replay->size(),native_replay->applied(),native_replay->size()-native_replay->applied());
        };
        const auto same_io=[&] {
            if(!enable_io) return true;
            const auto& a=native_io.chip().state();const auto& b=reference_io.chip().state();
            const auto& ni=native_io.inputs;const auto& ri=reference_io.inputs;
            return ni.in0==ri.in0 && ni.in1==ri.in1 && ni.in2==ri.in2 && ni.switches==ri.switches && ni.analog==ri.analog &&
                a.port_value==b.port_value && a.port_config==b.port_config &&
                a.mode==b.mode && a.analog_channel==b.analog_channel &&
                native_io.drive_command()==reference_io.drive_command() &&
                native_io.lamp_output()==reference_io.lamp_output() &&
                native_io.eeprom_mux()==reference_io.eeprom_mux() &&
                native_eeprom.write_enabled()==reference_eeprom.write_enabled() &&
                native_eeprom.save_image()==reference_eeprom.save_image();
        };
#else
        if(enable_io) throw rt::Fatal("I/O support not linked into test");
        const auto same_io=[] { return true; };
        const auto replay_inputs=[] {};
        const auto report_inputs=[] {};
#endif
#ifdef M2_SRALLY_TGP
        std::unique_ptr<rt::TgpBoard> nt,rtgp;
        std::unique_ptr<NativeTgpModel> nm,rm;
        if(positional_argc>=6) {
            auto tables=load(argv[4]),data=load(argv[5]);
            nt=std::make_unique<rt::TgpBoard>(tables,data);
            rtgp=std::make_unique<rt::TgpBoard>(tables,data);
            nm=std::make_unique<NativeTgpModel>(*nt);rm=std::make_unique<NativeTgpModel>(*rtgp);
            nb.board.add_model(nm.get());rb.board.add_model(rm.get());
        }
#else
        if(positional_argc>=6) throw rt::Fatal("TGP support not linked into test");
#endif
#if defined(M2_SRALLY_VIDEO) && defined(M2_SRALLY_TGP)
        std::unique_ptr<rt::Geo> native_geo,reference_geo;
        std::unique_ptr<rt::Model2AVideo> native_video,reference_video;
        if(enable_video) {
            auto polygons=load(argv[6]),textures=load(argv[7]);
            native_geo=std::make_unique<rt::Geo>(polygons,textures,nt->buffer_data());
            reference_geo=std::make_unique<rt::Geo>(polygons,textures,rtgp->buffer_data());
            native_video=std::make_unique<rt::Model2AVideo>(rt::Model2AVideo::Hooks{
                [&] { return enable_screen?native_screen->latched_frame():uint64_t(0); },[&] { return uint32_t(native_geo->polys.size()); },
                [&](uint32_t value) { native_geo->zclip_w(value); }});
            reference_video=std::make_unique<rt::Model2AVideo>(rt::Model2AVideo::Hooks{
                [&] { return enable_screen?reference_screen->latched_frame():uint64_t(0); },[&] { return uint32_t(reference_geo->polys.size()); },
                [&](uint32_t value) { reference_geo->zclip_w(value); }});
            nb.board.add_model(native_video.get());rb.board.add_model(reference_video.get());
            std::printf("Video parity independent_devices=true actual_geo_bound=true screen_connected=%s hardware_rendering_verified=false\n",enable_screen?"true":"false");
        }
#ifdef M2_SRALLY_CONTROL
        if(enable_screen) {
            // Pinned model2.cpp screen_vblank: screen has already latched its
            // frame, then geometry conditional on video_control bit0/parity,
            // then IRQ request, then communication-board vblank notification.
            native_screen->on_begin=[&](uint64_t deadline,uint64_t frame) {
                native_frames.emplace_back(true,deadline,frame);
                if((native_video->video_control()&1)==0 || (frame&1)==0) {
                    native_geo->zclip_w(native_video->zclip());native_geo->parse(nt->geo_read_start());
                    native_video->frame_start();native_parses++;
                }
                native_control.vblank();
#ifdef M2_SRALLY_COMM
                if(enable_comm) native_comm.vblank();
#endif
            };
            reference_screen->on_begin=[&](uint64_t deadline,uint64_t frame) {
                reference_frames.emplace_back(true,deadline,frame);
                if((reference_video->video_control()&1)==0 || (frame&1)==0) {
                    reference_geo->zclip_w(reference_video->zclip());reference_geo->parse(rtgp->geo_read_start());
                    reference_video->frame_start();reference_parses++;
                }
                reference_control.vblank();
#ifdef M2_SRALLY_COMM
                if(enable_comm) reference_comm.vblank();
#endif
            };
            // screen_frame increments in Model2AScreen only after rendering.
            native_screen->on_end=[&](uint64_t deadline,uint64_t frame) {
                native_frames.emplace_back(false,deadline,frame);
                native_video->screen_update(native_geo->polys,native_geo->windows());native_renders++;
            };
            reference_screen->on_end=[&](uint64_t deadline,uint64_t frame) {
                reference_frames.emplace_back(false,deadline,frame);
                reference_video->screen_update(reference_geo->polys,reference_geo->windows());reference_renders++;
            };
        }
#else
        if(enable_screen) throw rt::Fatal("screen requires linked control support");
#endif
        const auto same_video=[&](bool full=false) {
            if(!enable_video) return true;
            if(native_video->video_control()!=reference_video->video_control() ||
               native_video->zclip()!=reference_video->zclip() ||
               native_video->render_mode()!=reference_video->render_mode() ||
               native_video->render_test_mode()!=reference_video->render_test_mode()) return false;
            // Cycle-stamped CPU writes/reads establish each step's behavior;
            // compare all backing RAM at the final frontier as an additional
            // state check, without mutating handler latches through reads.
            if(!full) return true;
            native_screen_hash=native_video->video().screen_hash();
            reference_screen_hash=reference_video->video().screen_hash();
            const auto equal=[](auto a,auto b) {
                return a.size()==b.size() && !std::memcmp(a.data(),b.data(),a.size_bytes());
            };
            return equal(native_video->tile_ram(),reference_video->tile_ram()) &&
                equal(native_video->character_ram(),reference_video->character_ram()) &&
                equal(native_video->palette_ram(),reference_video->palette_ram()) &&
                equal(native_video->color_translation_ram(),reference_video->color_translation_ram()) &&
                equal(native_video->luma_ram(),reference_video->luma_ram()) &&
                equal(native_video->texture_words(0),reference_video->texture_words(0)) &&
                equal(native_video->texture_words(1),reference_video->texture_words(1)) &&
                equal(native_video->framebuffer_ram(0),reference_video->framebuffer_ram(0)) &&
                equal(native_video->framebuffer_ram(1),reference_video->framebuffer_ram(1)) &&
                native_video->video().screen()==reference_video->video().screen() && native_screen_hash==reference_screen_hash;
        };
#else
        if(enable_video) throw rt::Fatal("video and TGP support not linked into test");
        const auto same_video=[](bool=false) { return true; };
#endif
        native.reset();ref.reset();
        if(!same(native,ref) || nb.writes!=rb.writes) throw rt::Fatal("reset parity mismatch");
        gen::Env env{native,ls};
        const uint64_t limit=std::stoull(argv[3]);
        uint64_t writes=0;
        while(ls.count<limit) {
            // A frame can finish during an opcode or while synchronizing its
            // following boundary/IRQ. Apply before the next opcode in either
            // case; independent cursors reject skipped or mismatched events.
            replay_inputs();
            // Include pending-table writes and IRQ entry before the opcode in
            // this step. Neither IRQ entry nor its 9 helper cycles is an opcode.
            nb.writes.clear();rb.writes.clear();
            native_irqs.events.clear();reference_irqs.events.clear();
            native_frames.clear();reference_frames.clear();
            clear_uart_events();
            const auto before_count=ls.count;
            ls.end_count=ls.count+1;
            sync_native();sync_reference();
            advance_control();
            replay_inputs();
            if(ls.count!=before_count || !same(native,ref) || nb.writes!=rb.writes ||
               native_irqs.events!=reference_irqs.events || !same_control() || !same_clock() || !same_uart())
                throw rt::Fatal("instruction boundary/IRQ parity mismatch");
            const auto pc=native.m_IP;
            if(!gen::has_code(pc)) {
                if(!same_video(true)) throw rt::Fatal("video RAM mismatch at code frontier");
                report_screen();
                report_uart();
                report_inputs();
                std::printf("PASS native/reference parity instructions=%llu writes=%llu cycles=%llu irq_entries=%llu frontier=missing_code pc=%08x\n",
                    (unsigned long long)ls.count,(unsigned long long)writes,(unsigned long long)native.reference_cycles,
                    (unsigned long long)native_irqs.total,pc);
                return 0;
            }
            ref.m_icount=0; // per-step oracle metric, avoiding signed accumulation overflow
            try {
                while(!ls.finished()) gen::run(env);
            } catch(const rt::Model2AAccess& access) {
                try { ref.execute_one(); }
                catch(const rt::Model2AAccess& other) {
                    // The interpreter advances IP before executing; generated
                    // code leaves IP at this instruction until it completes.
                    // Neither engine completed the failing instruction.
                    ref.m_IP=native.m_IP;
                    sync_native();sync_reference();
                    if(access.address==other.address && access.width==other.width &&
                       access.write==other.write && access.value==other.value &&
                       nb.writes==rb.writes && same(native,ref) && same_io() && same_comm() &&
                       same_control() && same_video(true) && same_clock() && native_irqs.events==reference_irqs.events &&
                       native_frames==reference_frames && native_parses==reference_parses && native_renders==reference_renders && same_uart()) {
                        report_screen();
                        report_uart();
                        report_inputs();
                        std::printf("PASS native/reference parity instructions=%llu writes=%llu cycles=%llu irq_entries=%llu frontier=device pc=%08x access=%s%u address=%08x value=%08x\n",
                            (unsigned long long)ls.count,(unsigned long long)writes,(unsigned long long)native.reference_cycles,
                            (unsigned long long)native_irqs.total,pc,
                            access.write?"write":"read",access.width*8,access.address,access.value);
                        return 0;
                    }
                    std::fprintf(stderr,"Frontier mismatch count=%llu pc=%08x native=%08x/%u/%d/%08x reference=%08x/%u/%d/%08x state=%d writes=%d\n",
                        (unsigned long long)ls.count,pc,access.address,access.width,access.write,access.value,
                        other.address,other.width,other.write,other.value,same(native,ref),nb.writes==rb.writes);
                    std::fprintf(stderr,"frontier reference cycles native=%llu oracle=%llu\n",
                        (unsigned long long)native.reference_cycles,(unsigned long long)ref.reference_cycles);
                }
                throw rt::Fatal("device frontier parity mismatch");
            }
            ref.execute_one();
            // Canonical end boundary: long instructions fetch displacement
            // bytes through the oracle bus while native code embeds them.
            // Synchronize both clocks before comparing pending queue state.
            sync_native();sync_reference();
            if(ls.count!=before_count+1 || !same(native,ref) || nb.writes!=rb.writes ||
               !same_io() || !same_comm() || !same_control() || !same_video() ||
               native_irqs.events!=reference_irqs.events || !same_clock() || native_frames!=reference_frames ||
               native_parses!=reference_parses || native_renders!=reference_renders || !same_uart()) {
                std::fprintf(stderr,"Parity mismatch count=%llu pc=%08x native_ip=%08x reference_ip=%08x\n",
                    (unsigned long long)ls.count,pc,native.m_IP,ref.m_IP);
                std::fprintf(stderr,"reference cycles native=%llu oracle=%llu write_timestamps_equal=%d\n",
                    (unsigned long long)native.reference_cycles,(unsigned long long)ref.reference_cycles,nb.writes==rb.writes);
                for(unsigned i=0;i<32;i++) if(native.m_r[i]!=ref.m_r[i])
                    std::fprintf(stderr,"r%u native=%08x reference=%08x\n",i,native.m_r[i],ref.m_r[i]);
                return 1;
            }
            writes+=nb.writes.size();
        }
        if(!same_video(true)) throw rt::Fatal("video RAM mismatch at instruction limit");
        report_screen();
        report_uart();
        report_inputs();
        std::printf("PASS native/reference parity instructions=%llu writes=%llu cycles=%llu irq_entries=%llu frontier=instruction_limit pc=%08x\n",
            (unsigned long long)ls.count,(unsigned long long)writes,(unsigned long long)native.reference_cycles,
            (unsigned long long)native_irqs.total,native.m_IP);
        return 0;
    } catch(const std::exception& e) { std::fprintf(stderr,"boot parity: %s\n",e.what());return 1; }
}
