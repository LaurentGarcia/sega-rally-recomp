// Bounded native startup using the same machine as the SDL3/Metal frontend.
#include "runtime/model2a_machine.h"
#include "input_replay.h"
#ifdef M2_SRALLY_VIDEO
#include "runtime/video.h"
#endif
#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <optional>
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
    int expected = 4;
#ifdef M2_SRALLY_TGP
    expected += 2;
#endif
#ifdef M2_SRALLY_VIDEO
    expected += 2;
#endif
#ifdef M2_SRALLY_SOUND
    expected += 2;
#endif
    if (argc < expected) {
        std::fprintf(stderr, "usage: srally_boot PROGRAM.bin MAIN_DATA.bin INSTRUCTION_LIMIT [compiled TGP/video/sound image pairs] [FRAME.ppm] [--input-replay FILE] [--instruction-boundary-hook]\n");
        return 2;
    }
    try {
        size_t used = 0;
        const std::string requested = argv[3];
        const uint64_t limit = std::stoull(requested, &used, 10);
        if (used != requested.size() || !limit || limit > 2000000000)
            throw std::runtime_error("instruction limit must be 1..2000000000");
        Machine::Images images;
        images.program = load(argv[1]); images.main_data = load(argv[2]);
        Machine::Options options{false, false, false, false, false, false, false, false};
        [[maybe_unused]] int next_image = 4;
        int next_option = expected;
        const char *capture = nullptr;
        if (next_option < argc && !std::string(argv[next_option]).starts_with("--"))
            capture = argv[next_option++];
        std::optional<srally::InputReplay> replay;
        while (next_option < argc) {
            const std::string argument = argv[next_option++];
            if (argument == "--input-replay" && !replay && next_option < argc) {
                std::ifstream file(argv[next_option++]);
                if (!file) throw std::runtime_error("cannot open input replay");
                replay.emplace(file);
            } else if (argument == "--instruction-boundary-hook" && !options.instruction_boundary_hook) {
                options.instruction_boundary_hook = true;
            } else throw std::runtime_error("invalid or duplicate native boot option: " + argument);
        }
#ifdef M2_SRALLY_TGP
        images.tgp_tables = load(argv[next_image++]);
        images.copro_data = load(argv[next_image++]); options.tgp = true;
#endif
#ifdef M2_SRALLY_VIDEO
        images.polygons = load(argv[next_image++]);
        images.textures = load(argv[next_image++]); options.video = true;
#endif
#ifdef M2_SRALLY_SOUND
        images.sound_program = load(argv[next_image++]);
        images.sound_samples = load(argv[next_image++]); options.sound = true;
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
        if (capture) throw std::runtime_error("frame capture requires timed screen rendering");
#endif
        if (replay && (!options.io || !options.screen))
            throw std::runtime_error("input replay requires timed screen rendering and cabinet I/O");
        Machine machine(std::move(images), options);
#ifdef M2_INSTRUCTION_BOUNDARY_HOOK
        std::printf("instruction_boundary_hook compiled=true enabled=%s\n",
            options.instruction_boundary_hook ? "true" : "false");
#endif
        const auto &cpu = machine.cpu();
        std::printf("reset IP=%08x SAT=%08x PRCB=%08x FP=%08x SP=%08x\n",
            cpu.m_IP, cpu.m_SAT, cpu.m_PRCB, cpu.m_r[rt::I960_FP], cpu.m_r[rt::I960_SP]);
        Machine::RunResult outcome;
        bool step_frames = replay.has_value();
#ifdef M2_SRALLY_STEP_FRAMES
        step_frames = true;
#endif
        do {
            const auto before = machine.stats();
            if (replay && replay->apply(before.frames_rendered, machine.inputs())) {
                const auto &pins = machine.inputs();
                std::printf("input_event completed_frames=%" PRIu64 " instructions=%" PRIu64
                    " cycles=%" PRIu64 " in0=%02x in1=%02x in2=%02x switches=%02x analog=",
                    before.frames_rendered, before.instructions, before.reference_cycles,
                    unsigned(pins.in0), unsigned(pins.in1), unsigned(pins.in2), unsigned(pins.switches));
                for (auto value : pins.analog) std::printf("%02x", unsigned(value));
                std::puts("");
            }
            outcome = step_frames ? machine.run_until_frame(limit - before.instructions) : machine.run_for(limit);
        } while (step_frames && outcome.reason == Machine::Reason::Frame && outcome.instructions < limit);
        if (replay) std::printf("input_replay events=%zu applied=%zu pending=%zu frame_origin=completed_renders\n",
            replay->size(), replay->applied(), replay->size() - replay->applied());
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
        if (options.uart) std::printf("native_uart linked=true clock_connected=true sound_board_connected=%s mode=%02x command=%02x tx_pin_changes=%" PRIu64 "\n",
            options.sound ? "true" : "false", unsigned(s.uart_mode), unsigned(s.uart_command), s.tx_pin_changes);
        if (options.sound) std::printf("native_sound linked=true timing=instruction_boundary bus_phase_verified=false instructions=%" PRIu64
            " cycles=%" PRIu64 " irq_entries=%" PRIu64 " pc=%06x previous_dispatch=%06x last_event_ns=%" PRIu64
            " samples=%" PRIu64 " dropped_samples=%" PRIu64 "\n",
            s.sound_instructions, s.sound_cycles, s.sound_irqs, s.sound_pc, s.sound_previous_dispatch,
            s.board_time_ns, s.sound_samples, s.sound_dropped_samples);
        if (options.video) std::printf("native_video linked=true frame_clock_connected=%s frames_rendered=%" PRIu64
            " frames_parsed=%" PRIu64 " latched_frame=%" PRIu64 " screen_frame=%" PRIu64 " screen_hash=%016" PRIx64 "\n",
            options.screen ? "true" : "false", s.frames_rendered, s.frames_parsed, s.latched_frame, s.screen_frame, s.screen_hash);
        if (options.video) std::printf("native_geometry polygons=%" PRIu64 " frames_with_geometry=%" PRIu64
            " first_geometry_frame=%" PRIu64 " frame_numbering=one_based_zero_means_none\n",
            s.polygons, s.frames_with_geometry, s.first_geometry_frame);
#ifdef M2_SRALLY_VIDEO
        if (capture && s.frames_rendered) {
            std::ofstream frame(capture, std::ios::binary);
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
