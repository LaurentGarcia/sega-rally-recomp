// Bounded execution of Rally's recompiled 68000 reset path. No sound chip
// reply is synthesized; unavailable SCSP accesses are reported as frontiers.
#include "runtime/model2a_sound_bus.h"
#include "runtime/snd_gen_support.h"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
std::vector<uint8_t> load(const char *path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) throw rt::Fatal(std::string("cannot open ") + path);
    return {std::istreambuf_iterator<char>(file), {}};
}
void state(const snd::Cpu68k &cpu, const snd::Sched &schedule, const snd::Model2ASoundBus &bus,
           uint32_t reset_pc, uint32_t reset_sp)
{
    const auto ram = bus.sound_ram();
    const auto nonzero = std::count_if(ram.begin(), ram.end(), [](uint8_t value) { return value != 0; });
    std::printf("\"pc\":%u,\"sr\":%u,\"instructions\":%llu,\"reset_pc\":%u,"
                "\"reset_ssp\":%u,\"ram_nonzero_bytes\":%zu,\"board_time_modeled\":false,\"d\":[",
                cpu.pc, cpu.sr(), (unsigned long long)schedule.count, reset_pc, reset_sp, size_t(nonzero));
    for (unsigned i = 0; i < 8; ++i) std::printf("%s%u", i ? "," : "", cpu.d[i]);
    std::printf("],\"a\":[");
    for (unsigned i = 0; i < 8; ++i) std::printf("%s%u", i ? "," : "", cpu.a[i]);
    std::printf("]}");
}
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        std::fprintf(stderr, "usage: srally_sound_boot AUDIOCPU.bin SAMPLES.bin MAX_INSTRUCTIONS\n");
        return 2;
    }
    try {
        uint64_t limit = 0;
        const std::string number = argv[3];
        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), limit);
        if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() || !limit)
            throw rt::Fatal("invalid sound instruction limit");
        const auto program = load(argv[1]), samples = load(argv[2]);
        snd::Model2ASoundBus bus(program, samples);
        snd::Cpu68k cpu;
        cpu.bus = &bus;
        cpu.reset();
        const auto reset_pc = cpu.pc, reset_sp = cpu.a[7];
        snd::Sched schedule(cpu);
        schedule.auto_irq = false; // no chip/IRQ source is connected yet
        schedule.end_count = limit;
        sndgen::Env env{cpu, schedule};
        try {
            while (schedule.count < limit) {
                if (!sndgen::has_code(cpu.pc)) {
                    std::printf("{\"status\":\"missing_code\",");
                    state(cpu, schedule, bus, reset_pc, reset_sp);
                    std::puts("");
                    return 3;
                }
                sndgen::run(env);
            }
            std::printf("{\"status\":\"instruction_limit\",");
            state(cpu, schedule, bus, reset_pc, reset_sp);
            std::puts("");
            return 4;
        } catch (const snd::Model2ASoundBus::Frontier &frontier) {
            std::printf("{\"status\":\"frontier\",\"region\":\"%s\",\"write\":%s,"
                        "\"address\":%u,\"data\":%u,\"mask\":%u,",
                        frontier.region == snd::Model2ASoundBus::Frontier::Region::Scsp ? "scsp" : "unmapped",
                        frontier.write ? "true" : "false", frontier.address, frontier.data, frontier.mask);
            state(cpu, schedule, bus, reset_pc, reset_sp);
            std::puts("");
            return 3;
        }
    } catch (const std::exception &failure) {
        std::fprintf(stderr, "srally_sound_boot: %s\n", failure.what());
        return 2;
    }
}
