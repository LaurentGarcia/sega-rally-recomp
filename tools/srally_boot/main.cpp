// Bounded native i960 bring-up, using the same Cpu/Lockstep services as Daytona.
// A device not implemented in Model2ABus is an explicit frontier, not a pass.
#include "runtime/gen_support.h"
#include "runtime/model2a_bus.h"
#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
std::vector<uint8_t> load(const char *path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error(std::string("cannot read ") + path);
    return {std::istreambuf_iterator<char>(file), {}};
}
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        std::fprintf(stderr, "usage: srally_boot PROGRAM.bin MAIN_DATA.bin INSTRUCTION_LIMIT\n");
        return 2;
    }
    try {
        size_t used = 0;
        const std::string requested = argv[3];
        uint64_t limit = std::stoull(requested, &used, 10);
        if (used != requested.size() || !limit || limit > 100000000)
            throw std::runtime_error("instruction limit must be 1..100000000");
        rt::Model2ABus bus(load(argv[1]), load(argv[2]));
        rt::Cpu cpu(&bus);
        rt::Lockstep schedule(cpu);
        schedule.end_count = limit;
        gen::Env env{cpu, schedule};
        cpu.reset();
        std::printf("reset IP=%08x SAT=%08x PRCB=%08x FP=%08x SP=%08x\n",
            cpu.m_IP, cpu.m_SAT, cpu.m_PRCB, cpu.m_r[rt::I960_FP], cpu.m_r[rt::I960_SP]);
        try {
            while (!schedule.finished()) {
                if (!gen::has_code(cpu.m_IP)) {
                    std::printf("status=missing_code instructions=%" PRIu64 " pc=%08x\n",
                        schedule.count, cpu.m_IP);
                    return 4;
                }
                gen::run(env);
            }
        } catch (const rt::Model2AAccess &access) {
            std::printf("status=unimplemented_device instructions=%" PRIu64
                " pc=%08x access=%s%u address=%08x value=%08x\n",
                schedule.count, cpu.m_IP, access.write ? "write" : "read",
                access.width * 8, access.address, access.value);
            return 3;
        } catch (const rt::Fatal &failure) {
            std::printf("status=runtime_error instructions=%" PRIu64 " pc=%08x message=%s\n",
                schedule.count, cpu.m_IP, failure.what());
            return 1;
        }
        std::printf("status=instruction_limit instructions=%" PRIu64 " pc=%08x\n",
            schedule.count, cpu.m_IP);
        return 0;
    } catch (const std::exception &failure) {
        std::fprintf(stderr, "srally_boot: %s\n", failure.what());
        return 2;
    }
}
