// Bounded native startup using the same machine as the SDL3/Metal frontend.
#include "runtime/model2a_machine.h"
#ifdef M2_SRALLY_VIDEO
#include "runtime/video.h"
#endif
#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace {
std::vector<uint8_t> load(const char *path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error(std::string("cannot read ") + path);
    return {std::istreambuf_iterator<char>(file), {}};
}
}

int main(int argc, char **argv) {
    using Machine = rt::Model2AMachine;
    if (argc != 4 && argc != 6 && argc != 8 && argc != 9) {
        std::fprintf(stderr, "usage: srally_boot PROGRAM.bin MAIN_DATA.bin INSTRUCTION_LIMIT [TGP_TABLES.bin COPRO_DATA.bin [POLYGONS.bin TEXTURES.bin [FRAME.ppm]]]\n");
        return 2;
    }
    try {
        size_t used = 0;
        const std::string requested = argv[3];
        const uint64_t limit = std::stoull(requested, &used, 10);
        if (used != requested.size() || !limit || limit > 1000000000)
            throw std::runtime_error("instruction limit must be 1..1000000000");
        Machine::Images images;
        images.program = load(argv[1]); images.main_data = load(argv[2]);
        Machine::Options options{false, false, false, false, false, false, false, false};
#ifdef M2_SRALLY_TGP
        if (argc >= 6) { images.tgp_tables = load(argv[4]); images.copro_data = load(argv[5]); options.tgp = true; }
#else
        if (argc >= 6) throw std::runtime_error("this runner was built without a native TGP program");
#endif
#ifdef M2_SRALLY_VIDEO
        if (argc >= 8) { images.polygons = load(argv[6]); images.textures = load(argv[7]); options.video = true; }
#else
        if (argc >= 8) throw std::runtime_error("this runner was built without native video");
#endif
#ifdef M2_SRALLY_CONTROL
        options.control = true;
#endif
#ifdef M2_SRALLY_COMM
        options.comm = true;
#endif
#ifdef M2_SRALLY_IO
        options.io = true;
#endif
#ifdef M2_SRALLY_UART
        options.uart = true;
#endif
#ifdef M2_SRALLY_SCREEN
        options.screen = true;
#else
        if (argc == 9) throw std::runtime_error("frame capture requires timed screen rendering");
#endif
        Machine machine(std::move(images), options);
        const auto &cpu = machine.cpu();
        std::printf("reset IP=%08x SAT=%08x PRCB=%08x FP=%08x SP=%08x\n",
            cpu.m_IP, cpu.m_SAT, cpu.m_PRCB, cpu.m_r[rt::I960_FP], cpu.m_r[rt::I960_SP]);
        Machine::RunResult outcome;
#ifdef M2_SRALLY_STEP_FRAMES
        do { outcome = machine.run_until_frame(limit - machine.stats().instructions); }
        while (outcome.reason == Machine::Reason::Frame && outcome.instructions < limit);
#else
        outcome = machine.run_for(limit);
#endif
        const auto s = machine.stats(true);
        const auto &d = machine.diagnostics();
        std::printf("reference_unmapped reads=%" PRIu64 " writes=%" PRIu64 " last_write=%08x pcb_verified=false\n",
            d.reference_gap_reads, d.reference_gap_writes, d.reference_gap_last_write);
        if (options.control) std::printf("native_control linked=true timers_connected=true\n");
        else std::printf("irq_reset writes=%" PRIu64 " asynchronous_sources=false\n", d.irq_reset_writes);
        std::printf("reference_clock cycles=%" PRIu64 " nanoseconds=%" PRIu64 " pcb_verified=false\n",
            s.reference_cycles, s.reference_cycles * 40);
        std::printf("native_cpu irq_entries=%u previous_dispatch=%08x\n", s.irq_entries, s.previous_dispatch);
        if (options.comm) std::printf("native_comm linked=true peer_connected=false firmware_executed=false\n");
        if (options.io) std::printf("native_io linked=true clock_connected=true\n");
        if (options.uart) std::printf("native_uart linked=true clock_connected=true sound_board_connected=false mode=%02x command=%02x tx_pin_changes=%" PRIu64 "\n",
            unsigned(s.uart_mode), unsigned(s.uart_command), s.tx_pin_changes);
        if (options.video) std::printf("native_video linked=true frame_clock_connected=%s frames_rendered=%" PRIu64
            " frames_parsed=%" PRIu64 " latched_frame=%" PRIu64 " screen_frame=%" PRIu64 " screen_hash=%016" PRIx64 "\n",
            options.screen ? "true" : "false", s.frames_rendered, s.frames_parsed, s.latched_frame, s.screen_frame, s.screen_hash);
#ifdef M2_SRALLY_VIDEO
        if (argc == 9 && s.frames_rendered) {
            std::ofstream frame(argv[8], std::ios::binary);
            frame << "P6\n" << machine.video().width() << ' ' << rt::Video::H << "\n255\n";
            for (uint32_t pixel : machine.video().screen()) {
                const char rgb[] = {char(pixel >> 16), char(pixel >> 8), char(pixel)};
                frame.write(rgb, 3);
            }
            if (!frame) throw rt::Fatal("cannot write rendered frame capture");
            std::printf("frame_capture format=ppm width=%d height=%d rendered_frame=%" PRIu64 "\n",
                machine.video().width(), rt::Video::H, s.frames_rendered - 1);
        }
#endif
        if (options.tgp) std::printf("native_tgp linked=true booted=%s instructions=%" PRIu64 "\n",
            s.tgp_booted ? "true" : "false", s.tgp_instructions);
        if (outcome.reason == Machine::Reason::MissingCode) {
            std::printf("missing_context pfp=%08x sp=%08x rip=%08x g0=%08x g1=%08x g14=%08x fp=%08x\n",
                cpu.m_r[0], cpu.m_r[1], cpu.m_r[2], cpu.m_r[16], cpu.m_r[17], cpu.m_r[30], cpu.m_r[31]);
            std::printf("status=missing_code instructions=%" PRIu64 " pc=%08x\n", s.instructions, s.pc);
            return 4;
        }
        if (outcome.reason == Machine::Reason::DeviceAccess) {
            std::printf("status=unimplemented_device instructions=%" PRIu64 " pc=%08x access=%s%u address=%08x value=%08x\n",
                s.instructions, s.pc, outcome.write ? "write" : "read", outcome.width * 8, outcome.address, outcome.value);
            return 3;
        }
        if (outcome.reason == Machine::Reason::RuntimeError) {
            std::printf("status=runtime_error instructions=%" PRIu64 " pc=%08x message=%s\n",
                s.instructions, s.pc, outcome.message.c_str());
            return 1;
        }
        std::printf("status=instruction_limit instructions=%" PRIu64 " pc=%08x\n", s.instructions, s.pc);
        return 0;
    } catch (const std::exception &failure) {
        std::fprintf(stderr, "srally_boot: %s\n", failure.what());
        return 2;
    }
}
