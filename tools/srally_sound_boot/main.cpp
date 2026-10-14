// Bounded execution of Rally's recompiled 68000 reset path. No sound chip
// reply is synthesized; unavailable SCSP accesses are reported as frontiers.
#include "runtime/model2a_sound_bus.h"
#include "runtime/snd_gen_support.h"
#ifdef SEGARALLY_SOUND_TIMED_PROBE
#include "runtime/model2_scsp.h"
#endif
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <memory>

namespace {
#ifdef SEGARALLY_SOUND_TIMED_PROBE
struct ProbeScsp final:snd::Model2ASoundBus::Scsp {
    snd::Model2Scsp chip;
    uint64_t reads=0,writes=0;
    ProbeScsp(rt::Model2AClock &clock,std::span<uint8_t> ram):chip(clock,ram,snd::Model2Scsp::ReferenceRandom{},1024){}
    uint16_t read16(uint16_t address,uint16_t mask)override{++reads;return chip.read16(address,mask);}
    void write16(uint16_t address,uint16_t value,uint16_t mask)override{++writes;chip.write16(address,value,mask);}
};
ProbeScsp *active_scsp=nullptr;
#endif
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
                "\"reset_ssp\":%u,\"ram_nonzero_bytes\":%zu,\"board_time_modeled\":%s,\"d\":[",
                cpu.pc, cpu.sr(), (unsigned long long)schedule.count, reset_pc, reset_sp, size_t(nonzero),cpu.model2a_timing?"true":"false");
    for (unsigned i = 0; i < 8; ++i) std::printf("%s%u", i ? "," : "", cpu.d[i]);
    std::printf("],\"a\":[");
    for (unsigned i = 0; i < 8; ++i) std::printf("%s%u", i ? "," : "", cpu.a[i]);
    std::printf("]");
    if(cpu.model2a_timing) {
        const auto&s=cpu.cycle_stats;
        std::printf(",\"cycles\":%llu,\"time_ns\":%llu,\"timing_mode\":\"model2a-boundary\","
                    "\"mmio_at_previous_boundary\":true,\"bus_phase_verified\":false,"
                    "\"data_bus_transfers\":%llu,\"modeled_prefetch_transfers\":%llu,\"dummy_reads\":%llu,"
                    "\"mapped_wait_cycles\":%llu,\"iack_transfers\":%llu,\"iack_wait_cycles\":%llu",
                    (unsigned long long)cpu.cycles,(unsigned long long)snd::Cpu68k::model2a_time_ns(cpu.cycles),
                    (unsigned long long)s.data_transfers,(unsigned long long)s.prefetch_transfers,
                    (unsigned long long)s.dummy_reads,(unsigned long long)s.mapped_waits,
                    (unsigned long long)s.iack_transfers,(unsigned long long)s.iack_waits);
#ifdef SEGARALLY_SOUND_TIMED_PROBE
        if(active_scsp)std::printf(",\"scsp_reads\":%llu,\"scsp_writes\":%llu,\"sample_frames\":%llu,\"dropped_sample_frames\":%llu",
            (unsigned long long)active_scsp->reads,(unsigned long long)active_scsp->writes,
            (unsigned long long)active_scsp->chip.sample_count(),(unsigned long long)active_scsp->chip.dropped_samples());
#endif
    }
    std::printf("}");
}
}

int main(int argc, char **argv)
{
    if (argc != 4 && argc != 5) {
        std::fprintf(stderr, "usage: srally_sound_boot AUDIOCPU.bin SAMPLES.bin MAX_INSTRUCTIONS [--timed]\n");
        return 2;
    }
    try {
        uint64_t limit = 0;
        const std::string number = argv[3];
        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), limit);
        if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() || !limit)
            throw rt::Fatal("invalid sound instruction limit");
        const auto program = load(argv[1]), samples = load(argv[2]);
#ifdef SEGARALLY_SOUND_TIMED_PROBE
        rt::Model2AClock clock;
#endif
        snd::Model2ASoundBus bus(program, samples);
        snd::Cpu68k cpu;
        cpu.bus = &bus;
        snd::Sched schedule(cpu);
        schedule.auto_irq = false;
        const bool timed=argc==5 && std::string(argv[4])=="--timed";
        if(argc==5 && !timed)throw rt::Fatal("unknown sound probe option");
#ifdef SEGARALLY_SOUND_TIMED_PROBE
        std::unique_ptr<ProbeScsp> scsp;
        unsigned irq_lines=0,held_lines=0;
        auto update_irq=[&]{
            const int previous=cpu.irq_line;cpu.irq_line=0;
            for(int level=7;level>0;--level)if(irq_lines&(1u<<level)){cpu.irq_line=level;break;}
            if(previous!=7 && cpu.irq_line==7)cpu.nmi_pending=true;
            schedule.poke();
        };
        if(timed) {
            scsp=std::make_unique<ProbeScsp>(clock,bus.sound_ram());active_scsp=scsp.get();bus.set_scsp(scsp.get());
            cpu.model2a_timing=true;schedule.auto_irq=true;
            cpu.on_cycles=[&](uint64_t cycles){clock.advance(snd::Cpu68k::model2a_time_ns(cycles));};
            cpu.on_irq_ack=[&](int level){if(held_lines&(1u<<level)){held_lines&=~(1u<<level);irq_lines&=~(1u<<level);update_irq();}};
            scsp->chip.on_irq=[&](unsigned level,snd::Model2Scsp::LineState state){
                if(level>7)throw rt::Fatal("SCSP IRQ level outside 68000 IPL");
                if(state==snd::Model2Scsp::LineState::Clear){irq_lines&=~(1u<<level);held_lines&=~(1u<<level);}
                else {irq_lines|=1u<<level;if(state==snd::Model2Scsp::LineState::Hold)held_lines|=1u<<level;else held_lines&=~(1u<<level);}
                update_irq();
            };
        }
#else
        if(timed)throw rt::Fatal("this sound probe was built without timing/SCSP");
#endif
        cpu.reset();
        const auto reset_pc = cpu.pc, reset_sp = cpu.a[7];
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
        } catch(const rt::Fatal &failure) {
            std::printf("{\"status\":\"execution_frontier\",");
            state(cpu,schedule,bus,reset_pc,reset_sp);std::puts("");
            std::fprintf(stderr,"srally_sound_boot: %s\n",failure.what());return 3;
        }
    } catch (const std::exception &failure) {
        std::fprintf(stderr, "srally_sound_boot: %s\n", failure.what());
        return 2;
    }
}
