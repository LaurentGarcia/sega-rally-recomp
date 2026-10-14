// Bounded native i960 bring-up, using the same Cpu/Lockstep services as Daytona.
// A device not implemented in Model2ABus is an explicit frontier, not a pass.
#include "runtime/gen_support.h"
#include "runtime/model2a_bus.h"
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
#include <fstream>
#include <iterator>
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
    if (argc != 4 && argc != 6) {
        std::fprintf(stderr, "usage: srally_boot PROGRAM.bin MAIN_DATA.bin INSTRUCTION_LIMIT [TGP_TABLES.bin COPRO_DATA.bin]\n");
        return 2;
    }
    try {
        size_t used = 0;
        const std::string requested = argv[3];
        uint64_t limit = std::stoull(requested, &used, 10);
        if (used != requested.size() || !limit || limit > 100000000)
            throw std::runtime_error("instruction limit must be 1..100000000");
        rt::Model2ABus bus(load(argv[1]), load(argv[2]));
#ifdef M2_SRALLY_IO
        // A diagnostic frozen clock permits serial reads, but cannot complete
        // busy EEPROM writes. The board-cycle adapter is a separate frontier.
        const uint64_t board_time_ns = 0;
        rt::Model2Eeprom eeprom;
        eeprom.reset();
        rt::Model2AIO io(eeprom, [&] { return board_time_ns; });
        bus.add_model(&io);
#endif
#ifdef M2_SRALLY_TGP
        std::unique_ptr<rt::TgpBoard> tgp;
        std::unique_ptr<NativeTgpModel> tgp_model;
        if (argc == 6) {
            tgp = std::make_unique<rt::TgpBoard>(load(argv[4]), load(argv[5]));
            tgp_model = std::make_unique<NativeTgpModel>(*tgp);
            bus.add_model(tgp_model.get());
        }
#else
        if (argc == 6) throw std::runtime_error("this runner was built without a native TGP program");
#endif
        const auto report_contracts = [&] {
            const auto &d = bus.diagnostics();
            std::printf("reference_unmapped reads=%" PRIu64 " writes=%" PRIu64
                " last_write=%08x pcb_verified=false\n",
                d.reference_gap_reads, d.reference_gap_writes, d.reference_gap_last_write);
            std::printf("irq_reset writes=%" PRIu64 " asynchronous_sources=false\n", d.irq_reset_writes);
#ifdef M2_SRALLY_IO
            std::printf("native_io linked=true clock_ns=%llu clock_connected=false\n",
                (unsigned long long)board_time_ns);
#endif
#ifdef M2_SRALLY_TGP
            if (tgp) std::printf("native_tgp linked=true booted=%s instructions=%llu\n",
                tgp->booted() ? "true" : "false", (unsigned long long)tgp->tgp_instructions());
#endif
        };
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
                    report_contracts();
                    std::printf("status=missing_code instructions=%" PRIu64 " pc=%08x\n",
                        schedule.count, cpu.m_IP);
                    return 4;
                }
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
