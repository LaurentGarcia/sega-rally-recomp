// Bounded native i960 bring-up, using the same Cpu/Lockstep services as Daytona.
// A device not implemented in Model2ABus is an explicit frontier, not a pass.
#include "runtime/gen_support.h"
#include "runtime/model2a_bus.h"
#include "runtime/model2a_clock.h"
#ifdef M2_SRALLY_COMM
#include "runtime/model2a_comm.h"
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
#ifdef M2_SRALLY_IO
#include "runtime/model2a_io.h"
#include "runtime/model2_eeprom.h"
#endif
#ifdef M2_SRALLY_TGP
#include "runtime/m2_tgp_board.h"
#include <memory>
#endif
#include <cinttypes>
#include <cstdio>
#include <deque>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace {
#ifdef M2_SRALLY_TGP
// Native timing: complete pending FIFO work before reading buffer mailboxes,
// as M2Board does. Register, FIFO and geometry semantics stay in the shared
// TgpBoardModel; its trace-comparison policy is unused by this native bus.
class NativeTgpModel : public rt::TgpBoardModel {
public:
    explicit NativeTgpModel(rt::TgpBoard &board) : TgpBoardModel(board), board_(board) {}
    Check read(uint32_t address, uint32_t mask, uint32_t &value) override {
        if (address >= 0x00900000 && address <= 0x0097ffff) board_.sync();
        return TgpBoardModel::read(address, mask, value);
    }
private:
    rt::TgpBoard &board_;
};
#endif
std::vector<uint8_t> load(const char *path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error(std::string("cannot read ") + path);
    return {std::istreambuf_iterator<char>(file), {}};
}
}

int main(int argc, char **argv)
{
    if (argc != 4 && argc != 6 && argc != 8 && argc != 9) {
        std::fprintf(stderr, "usage: srally_boot PROGRAM.bin MAIN_DATA.bin INSTRUCTION_LIMIT [TGP_TABLES.bin COPRO_DATA.bin [POLYGONS.bin TEXTURES.bin [FRAME.ppm]]]\n");
        return 2;
    }
    try {
        size_t used = 0;
        const std::string requested = argv[3];
        uint64_t limit = std::stoull(requested, &used, 10);
        if (used != requested.size() || !limit || limit > 1000000000)
            throw std::runtime_error("instruction limit must be 1..1000000000");
        rt::Model2ABus bus(load(argv[1]), load(argv[2]));
        rt::Cpu cpu(&bus);
        rt::Model2AClock board_clock;
        bool synchronizing = false;
        const auto synchronize = [&] {
            // Device callbacks may access the bus at their exact event time.
            // They must not recursively advance to the CPU's later cursor.
            if (synchronizing) return;
            if (cpu.reference_cycles > std::numeric_limits<uint64_t>::max() / 40)
                throw rt::Fatal("Model2A reference clock overflow");
            synchronizing = true;
            try { board_clock.advance(cpu.reference_cycles * 40); }
            catch (...) { synchronizing = false; throw; }
            synchronizing = false;
        };
        bus.set_before_access(synchronize);
#ifdef M2_SRALLY_CONTROL
        rt::Model2AControl control(board_clock);
        bus.add_model(&control);
        std::deque<std::pair<unsigned, bool>> irq_edges;
        control.on_irq_line = [&](unsigned line, bool asserted) {
            irq_edges.emplace_back(line, asserted);
        };
#endif
#ifdef M2_SRALLY_SCREEN
        rt::Model2AScreen screen(board_clock);
        uint64_t frames_rendered = 0, frames_parsed = 0;
#endif
#ifdef M2_SRALLY_UART
        rt::Model2UART uart_chip;
        rt::Model2AUART uart(uart_chip);
        uint64_t tx_pin_changes = 0;
        bool tx_pin_level = true;
        rt::Model2UART::Callbacks uart_callbacks;
        uart_callbacks.tx_ready = uart_callbacks.rx_ready = [&](bool) {
            control.sound_ready(uart_chip.tx_ready(), uart_chip.rx_ready());
        };
        uart_callbacks.txd = [&](bool level) {
            if (level != tx_pin_level) ++tx_pin_changes;
            tx_pin_level = level;
        };
        uart_chip.set_callbacks(std::move(uart_callbacks));
        uart.reset();
        bus.add_model(&uart);
        // Pinned clock_device starts low: high at 1us, low at 2us.
        // UART RX remains physically idle until a real sound board is wired.
        bool uart_clock_level = false;
        std::function<void(uint64_t)> uart_tick;
        uart_tick = [&](uint64_t) {
            uart_clock_level = !uart_clock_level;
            uart.clock_edge(uart_clock_level);
            board_clock.schedule_after(1000, uart_tick);
        };
        board_clock.schedule_after(1000, uart_tick);
#endif
#ifdef M2_SRALLY_COMM
        rt::Model2AComm comm;
        bus.add_model(&comm);
#endif
#ifdef M2_SRALLY_IO
        // Pinned reference i960 costs at 25 MHz, not measured PCB timing.
        // Individual opcode costs are charged before their device accesses.
        rt::Model2Eeprom eeprom;
        eeprom.reset();
        rt::Model2AIO io(eeprom, [&] { return board_clock.now_ns(); });
        bus.add_model(&io);
#endif
#ifdef M2_SRALLY_TGP
        std::unique_ptr<rt::TgpBoard> tgp;
        std::unique_ptr<NativeTgpModel> tgp_model;
        if (argc >= 6) {
            tgp = std::make_unique<rt::TgpBoard>(load(argv[4]), load(argv[5]));
            tgp_model = std::make_unique<NativeTgpModel>(*tgp);
            bus.add_model(tgp_model.get());
        }
#else
        if (argc >= 6) throw std::runtime_error("this runner was built without a native TGP program");
#endif
#ifdef M2_SRALLY_VIDEO
        std::unique_ptr<rt::Geo> geo;
        std::unique_ptr<rt::Model2AVideo> video;
        if (argc >= 8) {
            if (!tgp) throw std::runtime_error("native video requires the shared TGP buffer");
            geo = std::make_unique<rt::Geo>(load(argv[6]), load(argv[7]), tgp->buffer_data());
            video = std::make_unique<rt::Model2AVideo>(rt::Model2AVideo::Hooks{
#ifdef M2_SRALLY_SCREEN
                [&] { return screen.latched_frame(); },
#else
                [] { return uint64_t(0); }, // Explicit diagnostic frame counter.
#endif
                [&] { return uint32_t(geo->polys.size()); },
                [&](uint32_t value) { geo->zclip_w(value); }});
            bus.add_model(video.get());
        }
#else
        if (argc >= 8) throw std::runtime_error("this runner was built without native video");
#endif
#ifdef M2_SRALLY_SCREEN
        if (!video || !geo || !tgp) throw std::runtime_error("screen timing requires native video and TGP");
        screen.on_begin = [&](uint64_t, uint64_t frame) {
            if (!(video->video_control() & 1) || !(frame & 1)) {
                geo->zclip_w(video->zclip());
                geo->parse(tgp->geo_read_start());
                video->frame_start();
                ++frames_parsed;
            }
            control.vblank();
#ifdef M2_SRALLY_COMM
            comm.vblank();
#endif
        };
        screen.on_end = [&](uint64_t, uint64_t) {
            video->screen_update(geo->polys, geo->windows());
            ++frames_rendered;
        };
#else
        if (argc == 9) throw std::runtime_error("frame capture requires timed screen rendering");
#endif
        rt::Lockstep schedule(cpu);
        schedule.end_count = limit;
        uint32_t previous_dispatch = cpu.m_IP;
        const auto report_contracts = [&] {
            const auto &d = bus.diagnostics();
            std::printf("reference_unmapped reads=%" PRIu64 " writes=%" PRIu64
                " last_write=%08x pcb_verified=false\n",
                d.reference_gap_reads, d.reference_gap_writes, d.reference_gap_last_write);
#ifdef M2_SRALLY_CONTROL
            std::printf("native_control linked=true timers_connected=true\n");
#else
            std::printf("irq_reset writes=%" PRIu64 " asynchronous_sources=false\n", d.irq_reset_writes);
#endif
            std::printf("reference_clock cycles=%" PRIu64 " nanoseconds=%" PRIu64 " pcb_verified=false\n",
                cpu.reference_cycles, cpu.reference_cycles * 40);
            std::printf("native_cpu irq_entries=%d previous_dispatch=%08x\n",
                schedule.interrupts(), previous_dispatch);
#ifdef M2_SRALLY_COMM
            std::printf("native_comm linked=true peer_connected=false firmware_executed=false\n");
#endif
#ifdef M2_SRALLY_IO
            std::printf("native_io linked=true clock_connected=true\n");
#endif
#ifdef M2_SRALLY_UART
            std::printf("native_uart linked=true clock_connected=true sound_board_connected=false"
                " mode=%02x command=%02x tx_pin_changes=%" PRIu64 "\n",
                unsigned(uart_chip.mode()), unsigned(uart_chip.command()), tx_pin_changes);
#endif
#ifdef M2_SRALLY_VIDEO
#ifdef M2_SRALLY_SCREEN
            std::printf("native_video linked=true frame_clock_connected=true frames_rendered=%" PRIu64
                " frames_parsed=%" PRIu64 " latched_frame=%" PRIu64 " screen_frame=%" PRIu64
                " screen_hash=%016" PRIx64 "\n", frames_rendered, frames_parsed,
                screen.latched_frame(), screen.screen_frame(), video->video().screen_hash());
            if (argc == 9 && frames_rendered) {
                std::ofstream frame(argv[8], std::ios::binary);
                frame << "P6\n" << video->video().width() << ' ' << rt::Video::H << "\n255\n";
                for (uint32_t pixel : video->video().screen()) {
                    const char rgb[] = {char(pixel >> 16), char(pixel >> 8), char(pixel)};
                    frame.write(rgb, 3);
                }
                if (!frame) throw rt::Fatal("cannot write rendered frame capture");
                std::printf("frame_capture format=ppm width=%d height=%d rendered_frame=%" PRIu64 "\n",
                    video->video().width(), rt::Video::H, frames_rendered - 1);
            }
#else
            if (video) std::printf("native_video linked=true frame_clock_connected=false frames_rendered=0\n");
#endif
#endif
#ifdef M2_SRALLY_TGP
            if (tgp) std::printf("native_tgp linked=true booted=%s instructions=%llu\n",
                tgp->booted() ? "true" : "false", (unsigned long long)tgp->tgp_instructions());
#endif
        };
        gen::Env env{cpu, schedule};
        cpu.reset();
        std::printf("reset IP=%08x SAT=%08x PRCB=%08x FP=%08x SP=%08x\n",
            cpu.m_IP, cpu.m_SAT, cpu.m_PRCB, cpu.m_r[rt::I960_FP], cpu.m_r[rt::I960_SP]);
        try {
            while (schedule.count < limit) {
#ifdef M2_SRALLY_CONTROL
                // Bounded diagnostic execution services time between every
                // instruction. IRQ entry itself does not count as an opcode.
                synchronize();
                while (!irq_edges.empty()) {
                    const auto [line, asserted] = irq_edges.front();
                    irq_edges.pop_front();
                    cpu.execute_set_input(int(line), asserted ? 1 : 0);
                }
                cpu.check_immediate_irqs();
                schedule.end_count = schedule.count + 1;
#endif
                if (!gen::has_code(cpu.m_IP)) {
                    report_contracts();
                    std::printf("missing_context pfp=%08x sp=%08x rip=%08x g0=%08x g1=%08x g14=%08x fp=%08x\n",
                        cpu.m_r[0], cpu.m_r[1], cpu.m_r[2], cpu.m_r[16], cpu.m_r[17], cpu.m_r[30], cpu.m_r[31]);
                    std::printf("status=missing_code instructions=%" PRIu64 " pc=%08x\n",
                        schedule.count, cpu.m_IP);
                    return 4;
                }
                previous_dispatch = cpu.m_IP;
                gen::run(env);
            }
        } catch (const rt::Model2AAccess &access) {
            report_contracts();
            std::printf("status=unimplemented_device instructions=%" PRIu64
                " pc=%08x access=%s%u address=%08x value=%08x\n",
                schedule.count, cpu.m_IP, access.write ? "write" : "read",
                access.width * 8, access.address, access.value);
            return 3;
        } catch (const std::exception &failure) {
            report_contracts();
            std::printf("status=runtime_error instructions=%" PRIu64 " pc=%08x message=%s\n",
                schedule.count, cpu.m_IP, failure.what());
            return 1;
        }
        report_contracts();
        std::printf("status=instruction_limit instructions=%" PRIu64 " pc=%08x\n",
            schedule.count, cpu.m_IP);
        return 0;
    } catch (const std::exception &failure) {
        std::fprintf(stderr, "srally_boot: %s\n", failure.what());
        return 2;
    }
}
