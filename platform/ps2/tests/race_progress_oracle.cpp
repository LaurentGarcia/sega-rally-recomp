// Host-only test adapter. Program bytes come from the owner's verified image.
// This interpreter and address adapter must never be linked into the PS2 game.
#include "race_progress.h"
#include "refcore/i960_ref.h"
#include <array>
#include <bit>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

namespace {
using ps2::native::RaceProgress;
constexpr std::uint32_t ram_base = 0x200000, entry = 0x5bb8b8, end = 0x5bb93c;
constexpr std::uint32_t return_address = 0xdead00;
constexpr std::uint32_t remaining_address = 0x20b0b0;
constexpr std::uint32_t counter_address[] = {0x2020bc, 0x2020b8};
constexpr std::uint32_t bank_address[] = {0x20afb0, 0x2020d0};
constexpr std::uint32_t index_address[] = {0x20ac98, 0x2020c0};
struct Write {
    std::uint32_t address, value;
    bool operator==(const Write &) const = default;
};
struct Memory : rt::Bus {
    const std::vector<std::uint8_t> &program;
    std::array<std::uint8_t, 65536> ram;
    std::vector<Write> writes;
    explicit Memory(const std::vector<std::uint8_t> &image) : program(image) {
        ram.fill(0x5a);
        writes.reserve(5);
    }
    std::uint32_t fetch(std::uint32_t address) override {
        if (address < entry || address >= end || (address & 3))
            throw rt::Fatal("progress oracle escaped its verified function");
        const auto offset = address - 0x59f000;
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(program.at(offset + i)) << (i * 8);
        return value;
    }
    std::uint8_t read_byte(std::uint32_t address) override { return ram.at(address - ram_base); }
    std::uint16_t read_word(std::uint32_t address) override {
        return read_byte(address) | (std::uint16_t(read_byte(address + 1)) << 8);
    }
    std::uint32_t read_dword(std::uint32_t address) override {
        return read_word(address) | (std::uint32_t(read_word(address + 2)) << 16);
    }
    void put(std::uint32_t address, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) ram.at(address - ram_base + i) = std::uint8_t(value >> (i * 8));
    }
    void write_byte(std::uint32_t, std::uint8_t) override { throw rt::Fatal("unexpected byte write"); }
    void write_word(std::uint32_t, std::uint16_t) override { throw rt::Fatal("unexpected word write"); }
    void write_dword(std::uint32_t address, std::uint32_t value) override {
        writes.push_back({address, value});
        put(address, value);
    }
    std::uint16_t flags(std::uint32_t) override { return rt::Cpu::BURST; }
    void set_state(const RaceProgress &state) {
        put(remaining_address, std::bit_cast<std::uint32_t>(state.remaining_ticks));
        for (unsigned i = 0; i < 2; ++i) put(counter_address[i], state.counters[i]);
        for (unsigned i = 0; i < RaceProgress::bank_size; ++i) {
            put(bank_address[0] + i * 4, state.first_bank[i]);
            put(bank_address[1] + i * 4, state.second_bank[i]);
        }
    }
};
std::uint32_t random_state = 0x8a73c421;
std::uint32_t random_word() {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
std::uint64_t fingerprint = 14695981039346656037ull;
unsigned cases = 0, instructions = 0;

bool compare(const std::vector<std::uint8_t> &program, std::int32_t remaining,
             unsigned first, unsigned second) {
    RaceProgress state;
    state.remaining_ticks = remaining;
    for (auto &value : state.counters) value = random_word();
    for (auto &value : state.first_bank) value = random_word();
    for (auto &value : state.second_bank) value = random_word();
    // Regular wrap cases mixed with unrestricted unsigned values.
    if (!(cases % 3)) state.counters[0] = state.first_bank[first] = UINT32_MAX;
    if (!(cases % 5)) state.counters[1] = state.second_bank[second] = UINT32_MAX;
    Memory actual(program), expected(program);
    actual.set_state(state);
    actual.put(index_address[0], first);
    actual.put(index_address[1], second);
    expected.ram = actual.ram;
    if (!ps2::native::advance_race_progress(state, first, second)) return false;
    expected.set_state(state);
    rt::I960Ref reference(&actual);
    reference.m_IP = entry;
    for (auto &reg : reference.m_r) reg = random_word();
    reference.m_r[30] = return_address;
    reference.m_AC = random_word() & 7;
    unsigned executed = 0;
    while (reference.m_IP != return_address) {
        if (++executed > 23) return false;
        reference.execute_one();
    }
    const std::vector<Write> expected_writes = {
        {remaining_address, std::bit_cast<std::uint32_t>(state.remaining_ticks)},
        {counter_address[0], state.counters[0]}, {counter_address[1], state.counters[1]},
        {bank_address[0] + first * 4, state.first_bank[first]},
        {bank_address[1] + second * 4, state.second_bank[second]},
    };
    if (executed != (remaining > 0 ? 23u : 22u) || actual.ram != expected.ram ||
        actual.writes != expected_writes || reference.m_r[30] != 0) return false;
    for (const auto &write : actual.writes) {
        fingerprint = (fingerprint ^ write.address) * 1099511628211ull;
        fingerprint = (fingerprint ^ write.value) * 1099511628211ull;
    }
    ++cases;
    instructions += executed;
    return true;
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    std::ifstream stream(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> program{std::istreambuf_iterator<char>(stream), {}};
    if (program.size() != 2097152) return 2;
    const std::int32_t boundaries[] = {INT32_MIN, -2, -1, 0, 1, 2, INT32_MAX};
    try {
        for (unsigned first = 0; first < 64; ++first)
            for (unsigned second = 0; second < 64; ++second)
                for (auto remaining : boundaries)
                    if (!compare(program, remaining, first, second)) return 1;
        for (unsigned i = 0; i < 4096; ++i) {
            const auto remaining = std::bit_cast<std::int32_t>(random_word());
            const auto first = random_word() % 64, second = random_word() % 64;
            if (!compare(program, remaining, first, second)) return 1;
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr, "progress oracle failed: %s\n", error.what());
        return 1;
    }
    std::printf("NATIVE_PROGRESS_ORACLE PASS cases=%u instructions=%u writes=%u hash=%016llx\n",
                cases, instructions, cases * 5, (unsigned long long)fingerprint);
    return 0;
}
