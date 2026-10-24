// Offline differential only. Never linked into a native game executable.
#include "orientation.h"
#include "orientation_cases.h"
#include "refcore/i960_ref.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
using orientation_cases::Bits;
constexpr std::uint32_t relocation = 0x59f000;
constexpr std::uint32_t entry = relocation + 0x2e560, end = relocation + 0x2e638;
constexpr std::uint32_t fold = relocation + 0x27b28, fold_end = relocation + 0x27bc0;
struct Memory : rt::Bus {
    const std::vector<std::uint8_t> &program;
    std::uint32_t object = 0;
    Bits values{};
    unsigned reads = 0, writes = 0;
    explicit Memory(const std::vector<std::uint8_t> &image) : program(image) {}
    std::uint32_t fetch(std::uint32_t address) override {
        if ((address & 3) || !((address >= entry && address < end) ||
                               (address >= fold && address < fold_end)))
            throw rt::Fatal("orientation instruction escaped the verified ranges");
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(program.at(address - relocation + i)) << (8*i);
        return value;
    }
    unsigned channel(std::uint32_t address) {
        if (address < object + 0x38 || address > object + 0x40 || (address & 3))
            throw rt::Fatal("orientation touched data outside its three channels");
        return (address - object - 0x38) / 4;
    }
    std::uint32_t read_dword(std::uint32_t address) override { ++reads; return values[channel(address)]; }
    void write_dword(std::uint32_t address, std::uint32_t value) override { ++writes; values[channel(address)] = value; }
    std::uint8_t read_byte(std::uint32_t) override { throw rt::Fatal("unexpected orientation byte read"); }
    std::uint16_t read_word(std::uint32_t) override { throw rt::Fatal("unexpected orientation halfword read"); }
    void write_byte(std::uint32_t, std::uint8_t) override { throw rt::Fatal("unexpected orientation byte write"); }
    void write_word(std::uint32_t, std::uint16_t) override { throw rt::Fatal("unexpected orientation halfword write"); }
    std::uint16_t flags(std::uint32_t) override { return 0; }
};
} // namespace
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    std::ifstream stream(argv[1],std::ios::binary);
    const std::vector<std::uint8_t> program{std::istreambuf_iterator<char>(stream),{}};
    if (program.size() != 2097152) return 2;
    std::uint64_t cases = 0, instructions = 0, reads = 0, writes = 0;
    std::uint64_t fingerprint = 14695981039346656037ull;
    unsigned minimum = 128, maximum = 0;
    std::vector<Bits> visited;
    try {
        Memory memory(program);
        rt::I960Ref reference(&memory);
        const bool ok = orientation_cases::accepted([&](Bits input) {
            ps2::native::OrientationAngles native{std::bit_cast<float>(input[0]),
                std::bit_cast<float>(input[1]),std::bit_cast<float>(input[2])};
            if (!ps2::native::finalize_orientation(native)) return false;
            const Bits expected{std::bit_cast<std::uint32_t>(native.first),
                std::bit_cast<std::uint32_t>(native.middle),std::bit_cast<std::uint32_t>(native.third)};
            std::fill(std::begin(reference.m_r),std::end(reference.m_r),std::uint32_t(cases)^0xa51ce39fu);
            std::fill(std::begin(reference.m_fp),std::end(reference.m_fp),-17.25);
            memory.object = 0x100000 + (std::uint32_t(cases) & 255u)*256u;
            memory.values = input; memory.reads = memory.writes = 0;
            reference.m_r[15] = memory.object;
            reference.m_AC = 0x3f001000;
            reference.m_IP = entry;
            unsigned steps = 0;
            while (reference.m_IP != end) {
                if (++steps > 128) throw rt::Fatal("orientation instruction bound exceeded");
                reference.execute_one();
            }
            if (memory.values != expected || memory.writes < 3 || memory.writes > 5) {
                std::fprintf(stderr,"orientation oracle mismatch input=%08x,%08x,%08x\n",
                             input[0],input[1],input[2]);
                return false;
            }
            for (unsigned channel = 0; channel < 3; ++channel) {
                fingerprint = (fingerprint ^ input[channel])*1099511628211ull;
                fingerprint = (fingerprint ^ expected[channel])*1099511628211ull;
            }
            fingerprint = (fingerprint ^ steps)*1099511628211ull;
            minimum = std::min(minimum,steps); maximum = std::max(maximum,steps);
            ++cases; instructions += steps; reads += memory.reads; writes += memory.writes;
            visited.push_back(input);
            return true;
        });
        if (!ok) return 1;
    } catch (const std::exception &error) {
        std::fprintf(stderr,"orientation oracle failed: %s\n",error.what()); return 1;
    }
    std::sort(visited.begin(),visited.end());
    const auto unique = std::unique(visited.begin(),visited.end()) - visited.begin();
    std::printf("NATIVE_ORIENTATION_ORACLE PASS cases=%llu unique_triples=%llu channel_operations=%llu instructions=%llu reads=%llu writes=%llu steps=%u..%u hash=%016llx\n",
        (unsigned long long)cases,(unsigned long long)unique,(unsigned long long)(cases*3),
        (unsigned long long)instructions,(unsigned long long)reads,(unsigned long long)writes,
        minimum,maximum,(unsigned long long)fingerprint);
    return 0;
}
