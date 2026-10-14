// Exercise the verified Rally upload through Daytona's native TGP board.
// This is a bring-up probe, not proof of complete command or race parity.
#include "runtime/m2_tgp_board.h"
#include "runtime/model2a_bus.h"
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
uint32_t word(const std::vector<uint8_t> &bytes, size_t offset)
{
    return uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8
         | uint32_t(bytes[offset + 2]) << 16 | uint32_t(bytes[offset + 3]) << 24;
}
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        std::fprintf(stderr, "usage: srally_tgp_boot PROGRAM.bin TABLES.bin COPRO_DATA.bin\n");
        return 2;
    }
    try {
        const auto program = load(argv[1]);
        if (program.empty() || program.size() % 4) throw std::runtime_error("invalid TGP program size");
        rt::TgpBoard board(load(argv[2]), load(argv[3]));
        rt::TgpBoardModel model(board);
        rt::Model2ABus bus({}, {});
        bus.set_model(&model);
        bus.write_dword(0x00980000, 0x80000000);
        for (size_t offset = 0; offset < program.size(); offset += 4)
            bus.write_dword(0x00884000, word(program, offset));
        bus.write_dword(0x00980000, 0); // existing board verifies size and CRC
        // The real uploader sends this function-port command immediately
        // after releasing reset (ROM 0x4c38), then reads one response.
        bus.write_dword(0x00880080, 0);
        const uint32_t response = bus.read_dword(0x00884000);
        const bool empty = bus.read_dword(0x00980004) != 0;
        std::printf("status=boot_probe booted=%s program_words=%zu response=%08x "
                    "output_empty=%s native_instructions=%llu full_tgp_parity=false\n",
                    board.booted() ? "true" : "false", program.size() / 4, response,
                    empty ? "true" : "false", (unsigned long long)board.tgp_instructions());
        return 0;
    } catch (const std::exception &failure) {
        std::fprintf(stderr, "srally_tgp_boot: %s\n", failure.what());
        return 1;
    }
}
