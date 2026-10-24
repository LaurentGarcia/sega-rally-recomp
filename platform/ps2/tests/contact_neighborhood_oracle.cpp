// Host-only differential against the owner's program. No original bytes here.
// The count response is an explicit fixture input; this does not execute TGP
// intersection firmware or validate a physical collision result.
#include "contact_neighborhood.h"
#include "refcore/i960_ref.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
using namespace ps2::native;
constexpr std::uint32_t relocation = 0x59f000;
constexpr std::uint32_t entry = relocation + 0x2abc0, end = relocation + 0x2acbc;
constexpr std::uint32_t descriptor = relocation + 0x3dac0;
constexpr std::uint32_t cursor_address = 0x20a290, fifo = 0x884000;
constexpr std::uint32_t scratch = 0x918000, point_address = 0x100000;
struct Write {
    std::uint32_t address, value;
    bool operator==(const Write &) const = default;
};
std::uint32_t word(const std::vector<std::uint8_t> &data, std::size_t offset) {
    return data.at(offset) | (std::uint32_t(data.at(offset + 1)) << 8) |
           (std::uint32_t(data.at(offset + 2)) << 16) | (std::uint32_t(data.at(offset + 3)) << 24);
}
struct Memory final : rt::Bus {
    const std::vector<std::uint8_t> &program;
    std::array<std::uint32_t, 3> point{};
    std::uint32_t count = 0, cursor = scratch, selector = 0;
    unsigned fifo_reads = 0, point_reads = 0, table_reads = 0;
    std::vector<Write> writes;
    explicit Memory(const std::vector<std::uint8_t> &image) : program(image) { writes.reserve(16); }
    std::uint32_t fetch(std::uint32_t address) override {
        if ((address & 3) || address < entry || address >= end)
            throw rt::Fatal("contact oracle escaped verified instruction range");
        return word(program, address - relocation);
    }
    std::uint32_t read_dword(std::uint32_t address) override {
        if (address == cursor_address) return cursor;
        if (address == fifo) { ++fifo_reads; return count; }
        if (!(address & 3) && address >= point_address && address < point_address + 12) {
            ++point_reads; return point[(address - point_address) / 4];
        }
        const auto selected = descriptor + 12 * selector;
        if (address == selected || address == selected + 4) {
            ++table_reads; return word(program, address - relocation);
        }
        throw rt::Fatal("contact oracle unexpected data read");
    }
    void write_dword(std::uint32_t address, std::uint32_t value) override {
        if (address != cursor_address && address != fifo &&
            !(!(address & 3) && address >= scratch && address < scratch + 40))
            throw rt::Fatal("contact oracle unexpected write");
        writes.push_back({address, value});
        if (address == cursor_address) cursor = value;
    }
    std::uint8_t read_byte(std::uint32_t) override { throw rt::Fatal("unexpected byte read"); }
    std::uint16_t read_word(std::uint32_t) override { throw rt::Fatal("unexpected halfword read"); }
    void write_byte(std::uint32_t, std::uint8_t) override { throw rt::Fatal("unexpected byte write"); }
    void write_word(std::uint32_t, std::uint16_t) override { throw rt::Fatal("unexpected halfword write"); }
    std::uint16_t flags(std::uint32_t) override { return 0; }
};
std::uint64_t cases = 0, instructions = 0, fingerprint = 14695981039346656037ull;
std::uint64_t accepted_cases = 0, rejected_cases = 0;
unsigned minimum_steps = 1000, maximum_steps = 0;

bool compare(Memory &memory, rt::I960Ref &reference, std::uint32_t count, std::int16_t current) {
    memory.count = count; memory.selector = std::uint32_t(cases) & 7;
    memory.cursor = scratch; memory.fifo_reads = memory.point_reads = memory.table_reads = 0;
    memory.writes.clear();
    memory.point = {0x7fc01234u ^ std::uint32_t(cases), 0x80000000u + std::uint32_t(cases),
                    std::uint32_t(cases) * 0x93791abdu};
    const std::uint32_t lane = std::uint32_t(cases) * 0x7f913517u;
    std::fill(std::begin(reference.m_r), std::end(reference.m_r), 0xabc09437u);
    reference.m_r[16] = memory.selector;
    reference.m_r[17] = static_cast<std::uint32_t>(static_cast<std::int32_t>(current));
    reference.m_r[18] = point_address; reference.m_r[19] = lane;
    reference.m_AC = 0x3f001000; reference.m_IP = entry;
    unsigned steps = 0;
    while (reference.m_IP != relocation + 0x2ac10 && reference.m_IP != relocation + 0x2acb8) {
        if (++steps > 64) throw rt::Fatal("contact oracle instruction bound exceeded");
        reference.execute_one();
    }
    constexpr ContactNeighborhood sentinel{601, 602, 603};
    auto result = sentinel;
    const bool accepted = contact_neighborhood(count, current, result);
    const bool original_accepted = reference.m_IP == relocation + 0x2acb8;
    if (accepted != original_accepted || (!accepted && result != sentinel)) return false;
    const auto table_offset = 0x3dac0 + 12 * memory.selector;
    std::vector<Write> expected{{scratch, word(memory.program, table_offset)},
                              {fifo, 0x03800707}, {cursor_address, scratch + 4}};
    if (accepted) {
        for (unsigned i = 0; i < 3; ++i) expected.push_back({scratch + 4 + 4 * i, memory.point[i]});
        expected.push_back({scratch + 16, word(memory.program, table_offset + 4)});
        expected.push_back({scratch + 20, count * 4});
        expected.push_back({scratch + 24, std::uint32_t(result.previous) * 4});
        expected.push_back({scratch + 28, std::uint32_t(result.current) * 4});
        expected.push_back({scratch + 32, std::uint32_t(result.next) * 4});
        expected.push_back({scratch + 36, lane << 1});
        expected.push_back({fifo, 0x29005252});
        expected.push_back({cursor_address, scratch + 40});
    }
    if (memory.writes != expected || memory.fifo_reads != 1 || memory.table_reads != 2 ||
        memory.point_reads != (accepted ? 3u : 0u)) return false;
    for (const auto &write : memory.writes) {
        fingerprint = (fingerprint ^ write.address) * 1099511628211ull;
        fingerprint = (fingerprint ^ write.value) * 1099511628211ull;
    }
    fingerprint = (fingerprint ^ count) * 1099511628211ull;
    fingerprint = (fingerprint ^ std::uint16_t(current)) * 1099511628211ull;
    minimum_steps = std::min(minimum_steps, steps); maximum_steps = std::max(maximum_steps, steps);
    ++cases; instructions += steps;
    accepted ? ++accepted_cases : ++rejected_cases;
    return true;
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    std::ifstream stream(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> program{std::istreambuf_iterator<char>(stream), {}};
    if (program.size() != 2097152) return 2;
    try {
        Memory memory(program);
        rt::I960Ref reference(&memory);
        for (const auto count : std::array<std::uint32_t, 8>{0, 1, 2, 10, 300, 32767, 32768, 65535})
            for (std::int32_t index = -32768; index <= 32767; ++index)
                if (!compare(memory, reference, count, static_cast<std::int16_t>(index))) {
                    std::fprintf(stderr, "contact oracle mismatch count=%u current=%d\n", count, index);
                    return 1;
                }
    } catch (const std::exception &error) {
        std::fprintf(stderr, "contact oracle failed: %s\n", error.what()); return 1;
    }
    std::printf("NATIVE_CONTACT_ORACLE PASS cases=%llu accepted=%llu rejected=%llu instructions=%llu steps=%u..%u hash=%016llx\n",
                static_cast<unsigned long long>(cases), static_cast<unsigned long long>(accepted_cases),
                static_cast<unsigned long long>(rejected_cases), static_cast<unsigned long long>(instructions),
                minimum_steps, maximum_steps, static_cast<unsigned long long>(fingerprint));
}
