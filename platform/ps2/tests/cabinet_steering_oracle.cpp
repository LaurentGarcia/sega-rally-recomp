// Host-only exhaustive comparison with the owner's verified program image.
#include "cabinet_steering.h"
#include "refcore/i960_ref.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
constexpr std::uint32_t entry = 0x395d8, end = 0x39668, output = 0x39664;
struct Memory final : rt::Bus {
    const std::vector<std::uint8_t> &program;
    std::uint8_t sample = 0;
    unsigned reads = 0;
    explicit Memory(const std::vector<std::uint8_t> &image) : program(image) {}
    std::uint32_t fetch(std::uint32_t address) override {
        if ((address & 3) || address < entry || address >= end)
            throw rt::Fatal("steering oracle escaped verified instruction range");
        return program.at(address) | (std::uint32_t(program.at(address + 1)) << 8) |
               (std::uint32_t(program.at(address + 2)) << 16) |
               (std::uint32_t(program.at(address + 3)) << 24);
    }
    std::uint8_t read_byte(std::uint32_t address) override {
        if (address != 0x202050) throw rt::Fatal("unexpected steering byte read");
        ++reads; return sample;
    }
    std::uint32_t read_dword(std::uint32_t) override { throw rt::Fatal("unexpected steering word read"); }
    std::uint16_t read_word(std::uint32_t) override { throw rt::Fatal("unexpected steering halfword read"); }
    void write_dword(std::uint32_t, std::uint32_t) override { throw rt::Fatal("unexpected steering write"); }
    void write_byte(std::uint32_t, std::uint8_t) override { throw rt::Fatal("unexpected steering byte write"); }
    void write_word(std::uint32_t, std::uint16_t) override { throw rt::Fatal("unexpected steering halfword write"); }
    std::uint16_t flags(std::uint32_t) override { return 0; }
};
} // namespace

int main(int argc, char **argv) {
    if (argc != 2 && argc != 3) return 2;
    std::ifstream stream(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> program{std::istreambuf_iterator<char>(stream), {}};
    if (program.size() != 2097152) return 2;
    std::uint64_t instructions = 0, hash = 14695981039346656037ull;
    unsigned minimum = 1000, maximum = 0;
    std::array<std::uint32_t, 256> original_results{};
    unsigned captured = 0;
    try {
        Memory memory(program);
        rt::I960Ref original(&memory);
        for (unsigned sample = 0; sample != 256; ++sample) {
            memory.sample = std::uint8_t(sample); memory.reads = 0;
            std::fill(std::begin(original.m_r), std::end(original.m_r), 0xabc01234u);
            original.m_r[30] = 0x123400;
            original.m_AC = 0x3f001000; original.m_IP = entry;
            unsigned steps = 0;
            while (original.m_IP != output) {
                if (++steps > 40) throw rt::Fatal("steering instruction bound exceeded");
                original.execute_one();
            }
            const auto actual = std::bit_cast<std::uint32_t>(ps2::native::cabinet_steering(memory.sample));
            if (actual != original.m_r[16] || memory.reads != 1 ||
                original.m_r[17] != 0x123400 || original.m_r[30] != 0) {
                std::fprintf(stderr,"steering mismatch sample=%u native=%08x original=%08x\n",
                             sample,actual,original.m_r[16]);
                return 1;
            }
            hash = (hash ^ sample) * 1099511628211ull;
            hash = (hash ^ actual) * 1099511628211ull;
            original_results[sample] = actual;
            instructions += steps; minimum = std::min(minimum,steps); maximum = std::max(maximum,steps);
        }
        if (argc == 3) {
            std::ifstream rows(argv[2]);
            if (!rows) throw rt::Fatal("cannot open steering capture inputs");
            unsigned sample = 0;
            std::uint32_t expected = 0;
            while (rows >> sample) {
                if (!(rows >> expected) || sample > 255 || ++captured > 16384 ||
                    expected != original_results[sample] || expected !=
                    std::bit_cast<std::uint32_t>(ps2::native::cabinet_steering(std::uint8_t(sample))))
                    throw rt::Fatal("live steering capture comparison failed");
            }
            if (!rows.eof() || !captured) throw rt::Fatal("invalid or empty steering capture inputs");
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr,"steering oracle failed: %s\n",error.what()); return 1;
    }
    std::printf("NATIVE_CABINET_STEERING_ORACLE PASS cases=256 instructions=%llu steps=%u..%u hash=%016llx live=%u\n",
                static_cast<unsigned long long>(instructions),minimum,maximum,
                static_cast<unsigned long long>(hash),captured);
}
