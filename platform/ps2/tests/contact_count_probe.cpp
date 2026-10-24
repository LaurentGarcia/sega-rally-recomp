// Host-only probe of the owner's TGP count command, never a native game source.
// It confirms the data relationship; no original firmware/data is embedded.
#include "runtime/tgp.h"
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
std::vector<std::uint8_t> read(const char *path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw rt::TgpFatal("cannot open count-probe input");
    return {std::istreambuf_iterator<char>(input), {}};
}
std::uint32_t word(const std::vector<std::uint8_t> &bytes, std::size_t offset) {
    return bytes.at(offset) | (std::uint32_t(bytes.at(offset + 1)) << 8) |
           (std::uint32_t(bytes.at(offset + 2)) << 16) | (std::uint32_t(bytes.at(offset + 3)) << 24);
}
struct Bus final : rt::TgpBus {
    const std::vector<std::uint8_t> &copro;
    std::array<std::uint32_t, 0x8000> buffer{};
    std::vector<std::uint32_t> input, output, reads;
    std::size_t consumed = 0;
    explicit Bus(const std::vector<std::uint8_t> &data) : copro(data) {}
    bool fifo_pop(std::uint32_t &value) override {
        if (consumed == input.size()) return false;
        value = input[consumed++]; return true;
    }
    void fifo_push(std::uint32_t value) override { output.push_back(value); }
    std::uint32_t mem_r(std::uint32_t address) override {
        reads.push_back(address);
        if (address & 0x800000) return word(copro, std::size_t(address & 0x1fffff) * 4);
        if (address & 0x400000) return buffer[address & 0x7fff];
        throw rt::TgpFatal("unexpected unmapped count-probe read");
    }
    void mem_w(std::uint32_t address, std::uint32_t value) override {
        if (!(address & 0x400000) || (address & 0x800000))
            throw rt::TgpFatal("unexpected count-probe write");
        buffer[address & 0x7fff] = value;
    }
};
void run(rt::Tgp &tgp, Bus &bus, std::uint32_t command) {
    bus.input.push_back(command);
    const auto bound = tgp.count + 10000;
    rt::tgpgen::run(tgp, bound);
    if (tgp.count >= bound || bus.consumed != bus.input.size())
        throw rt::TgpFatal("count-probe instruction/input bound failed");
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 6) return 2;
    try {
        const auto program = read(argv[1]), firmware = read(argv[2]), tables = read(argv[3]);
        const auto copro = read(argv[4]), main_data = read(argv[5]);
        if (program.size() != 2097152 || firmware.size() != 6660 || tables.size() != 262144 ||
            copro.size() != 8388608 || main_data.size() != 37748736)
            throw rt::TgpFatal("wrong count-probe input size");
        std::array<std::uint32_t, 0x10000> decoded_tables{};
        for (std::size_t i = 0; i < decoded_tables.size(); ++i) decoded_tables[i] = word(tables, i * 4);
        Bus bus(copro);
        rt::Tgp tgp;
        tgp.bus = &bus; tgp.tables = decoded_tables.data();
        for (std::size_t i = 0; i < firmware.size() / 4; ++i) tgp.prog[i] = word(firmware, i * 4);
        tgp.reset();
        run(tgp, bus, 0x04000000); // Normal firmware startup response.
        if (bus.output != std::vector<std::uint32_t>{8}) throw rt::TgpFatal("wrong firmware boot response");
        unsigned verified = 0;
        std::uint64_t hash = 14695981039346656037ull, query_instructions = 0;
        for (unsigned selector = 0; selector < 8; ++selector) {
            const auto source = word(program, 0x3dac0 + 12 * selector);
            const auto cpu_header = word(program, 0x3dac8 + 12 * selector);
            if (cpu_header < 0x2000000 || cpu_header > 0x2000000 + main_data.size() - 16)
                throw rt::TgpFatal("main-data header out of bounds");
            bus.buffer[0x6000 + selector] = source;
            bus.output.clear(); bus.reads.clear();
            const auto before = tgp.count;
            run(tgp, bus, 0x03800707);
            const auto steps = tgp.count - before;
            const auto expected = word(copro, std::size_t(source) * 4);
            std::vector<std::uint32_t> rom_reads;
            for (auto address : bus.reads) if (address & 0x800000) rom_reads.push_back(address);
            if (bus.output != std::vector<std::uint32_t>{expected} ||
                rom_reads != std::vector<std::uint32_t>{0x800000 | source} ||
                expected != word(main_data, cpu_header - 0x2000000))
                throw rt::TgpFatal("count command/header relationship mismatch");
            hash = (hash ^ selector) * 1099511628211ull;
            hash = (hash ^ expected) * 1099511628211ull;
            hash = (hash ^ steps) * 1099511628211ull;
            query_instructions += steps; ++verified;
            std::printf("CONTACT_COUNT selector=%u count=%u copro_word_reads=%zu instructions=%llu\n",
                        selector, expected, rom_reads.size(), static_cast<unsigned long long>(steps));
        }
        std::printf("NATIVE_CONTACT_COUNT PASS descriptors=%u query_instructions=%llu hash=%016llx\n",
                    verified, static_cast<unsigned long long>(query_instructions), static_cast<unsigned long long>(hash));
    } catch (const std::exception &error) {
        std::fprintf(stderr, "contact count probe failed: %s\n", error.what()); return 1;
    }
}
