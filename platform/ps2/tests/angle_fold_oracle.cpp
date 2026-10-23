// Host-only original-program differential; never linked into the native game.
#include "angle_fold.h"
#include "refcore/i960_ref.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
constexpr std::uint32_t entry = 0x5c6b28, end = 0x5c6bc0, sentinel = 0xdead00;
struct Memory : rt::Bus {
    const std::vector<std::uint8_t> &program;
    explicit Memory(const std::vector<std::uint8_t> &image) : program(image) {}
    std::uint32_t fetch(std::uint32_t address) override {
        if (address < entry || address >= end || (address & 3))
            throw rt::Fatal("angle oracle escaped the verified function");
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(program.at(address - 0x59f000 + i)) << (8 * i);
        return value;
    }
    std::uint8_t read_byte(std::uint32_t) override { throw rt::Fatal("unexpected angle data read"); }
    std::uint16_t read_word(std::uint32_t) override { throw rt::Fatal("unexpected angle data read"); }
    std::uint32_t read_dword(std::uint32_t) override { throw rt::Fatal("unexpected angle data read"); }
    void write_byte(std::uint32_t, std::uint8_t) override { throw rt::Fatal("unexpected angle write"); }
    void write_word(std::uint32_t, std::uint16_t) override { throw rt::Fatal("unexpected angle write"); }
    void write_dword(std::uint32_t, std::uint32_t) override { throw rt::Fatal("unexpected angle write"); }
    std::uint16_t flags(std::uint32_t) override { return 0; }
};
std::uint64_t cases = 0, instructions = 0, fingerprint = 14695981039346656037ull;
unsigned minimum_steps = 100, maximum_steps = 0;
bool compare(rt::I960Ref &reference, std::uint32_t bits) {
    float value = std::bit_cast<float>(bits);
    if (!ps2::native::fold_angle_once(value)) return false;
    const auto expected = std::bit_cast<std::uint32_t>(value);
    std::fill(std::begin(reference.m_r), std::end(reference.m_r), std::uint32_t(cases) ^ 0xa5c316e7u);
    std::fill(std::begin(reference.m_fp), std::end(reference.m_fp), -17.25);
    reference.m_r[16] = bits;
    reference.m_r[30] = sentinel;
    reference.m_AC = 0x3f001000; // Nearest/even; original exception masks.
    reference.m_IP = entry;
    unsigned executed = 0;
    while (reference.m_IP != sentinel) {
        if (++executed > 32) throw rt::Fatal("angle oracle instruction limit");
        reference.execute_one();
    }
    if (reference.m_r[16] != expected || reference.m_r[30] != 0) {
        std::fprintf(stderr, "angle oracle mismatch input=%08x native=%08x original=%08x\n",
                     bits, expected, reference.m_r[16]);
        return false;
    }
    minimum_steps = std::min(minimum_steps, executed);
    maximum_steps = std::max(maximum_steps, executed);
    fingerprint = (fingerprint ^ bits) * 1099511628211ull;
    fingerprint = (fingerprint ^ expected) * 1099511628211ull;
    fingerprint = (fingerprint ^ executed) * 1099511628211ull;
    ++cases; instructions += executed;
    return true;
}
bool both(rt::I960Ref &reference, std::uint32_t magnitude) {
    return compare(reference, magnitude) && compare(reference, magnitude | 0x80000000);
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 2 && argc != 3) return 2;
    const bool exhaustive = argc == 3 && std::strcmp(argv[2], "--exhaustive-folded") == 0;
    std::ifstream stream(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> program{std::istreambuf_iterator<char>(stream), {}};
    if (program.size() != 2097152) return 2;
    try {
        Memory memory(program);
        rt::I960Ref reference(&memory);
        constexpr std::uint32_t boundaries[] = {
            0, 1, 0x7fffff, 0x800000, 0x3f800000, 0x40490fd9,
            0x40490fda, 0x40490fdb, 0x40490fdc, 0x40800000, 0x40c90fda, 0x40c90fdb
        };
        for (auto value : boundaries) if (!both(reference, value)) return 1;
        for (unsigned exponent = 0; exponent <= 128; ++exponent)
            for (unsigned fraction : {0u, 1u, 0x155555u, 0x400000u, 0x7ffffeu, 0x7fffffu}) {
                const auto bits = (exponent << 23) | fraction;
                if (bits <= 0x40c90fdb && !both(reference, bits)) return 1;
            }
        std::uint32_t random = 0x84f3a915;
        for (unsigned i = 0; i < 32768; ++i) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            if (!both(reference, random % 0x40c90fdc)) return 1;
        }
        const unsigned step = exhaustive ? 1 : 127;
        for (std::uint32_t bits = 0x40490fdb; bits <= 0x40c90fdb; bits += step)
            if (!both(reference, bits)) return 1;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "angle oracle failed: %s\n", error.what());
        return 1;
    }
    std::printf("NATIVE_ANGLE_ORACLE PASS cases=%llu instructions=%llu steps=%u..%u hash=%016llx exhaustive_folded=%u\n",
                (unsigned long long)cases, (unsigned long long)instructions, minimum_steps, maximum_steps,
                (unsigned long long)fingerprint, unsigned(exhaustive));
    return 0;
}
