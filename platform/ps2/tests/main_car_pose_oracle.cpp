// Host-only original-program comparison. Never linked into the native game.
#include "main_car_pose_cases.h"
#include "refcore/i960_ref.h"
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <vector>

namespace {
using namespace ps2::native;
constexpr std::uint32_t ram_base = 0x200000, external_base = 0x2138a0;
constexpr std::uint32_t entry = 0x5d3078, end = 0x5d3130, sentinel = 0xdead00;
constexpr std::array<unsigned, 18> source_offsets{
    0x14, 0x18, 0x1c, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x40,
    0x74, 0x80, 0x84, 0x64, 0x68, 0x6c, 0x70, 0x10, 0x88};
struct Access {
    std::uint32_t address, value;
    bool operator==(const Access &) const = default;
};
struct Memory final : rt::Bus {
    const std::vector<std::uint8_t> &program;
    std::array<std::uint8_t, 1024> ram{};
    std::array<std::uint32_t, 4> external{};
    std::vector<Access> reads, writes;
    explicit Memory(const std::vector<std::uint8_t> &image) : program(image) {
        for (unsigned i = 0; i < ram.size(); ++i) ram[i] = std::uint8_t(i ^ 0xa5u);
        reads.reserve(23); writes.reserve(22);
    }
    std::uint32_t fetch(std::uint32_t address) override {
        if (address < entry || address >= end || (address & 3))
            throw rt::Fatal("main pose fetch escaped verified helper");
        std::uint32_t word = 0;
        for (unsigned i = 0; i < 4; ++i)
            word |= std::uint32_t(program.at(address - 0x59f000 + i)) << (8 * i);
        return word;
    }
    std::uint32_t get(std::uint32_t address) const {
        if (address & 3) throw rt::Fatal("unaligned main pose access");
        if (address >= external_base && address < external_base + 16)
            return external.at((address - external_base) / 4);
        if (address < ram_base || address > ram_base + ram.size() - 4)
            throw rt::Fatal("main pose data read escaped bounded inputs");
        std::uint32_t word = 0;
        for (unsigned i = 0; i < 4; ++i) word |= std::uint32_t(ram.at(address - ram_base + i)) << (8 * i);
        return word;
    }
    void put(std::uint32_t address, std::uint32_t value) {
        if ((address & 3) || address < ram_base || address > ram_base + ram.size() - 4)
            throw rt::Fatal("main pose write escaped bounded destination");
        for (unsigned i = 0; i < 4; ++i) ram.at(address - ram_base + i) = std::uint8_t(value >> (8 * i));
    }
    std::uint32_t read_dword(std::uint32_t address) override {
        const auto value = get(address); reads.push_back({address, value}); return value;
    }
    std::uint8_t read_byte(std::uint32_t) override { throw rt::Fatal("unexpected main pose byte read"); }
    std::uint16_t read_word(std::uint32_t) override { throw rt::Fatal("unexpected main pose halfword read"); }
    void write_dword(std::uint32_t address, std::uint32_t value) override {
        writes.push_back({address, value}); put(address, value);
    }
    void write_byte(std::uint32_t, std::uint8_t) override { throw rt::Fatal("unexpected main pose byte write"); }
    void write_word(std::uint32_t, std::uint16_t) override { throw rt::Fatal("unexpected main pose halfword write"); }
    std::uint16_t flags(std::uint32_t) override { return rt::Cpu::BURST; }
};
std::uint32_t cases = 0, instructions = 0, random_state = 0x678ba129u;
std::uint64_t fingerprint = 14695981039346656037ull;
std::set<main_car_pose_cases::Words> distinct_inputs;

bool compare(const std::vector<std::uint8_t> &program, const main_car_pose_cases::Case &c) {
    // Four legal word-alignment residues exercise original paired transfers.
    const auto node = ram_base + 64 + (cases % 4) * 4;
    const auto destination = ram_base + 512 + ((cases / 4) % 4) * 4;
    Memory actual(program), expected(program);
    for (unsigned i = 0; i < source_offsets.size(); ++i)
        actual.put(node + source_offsets[i], c.expected[i < 16 ? i : i + 4]);
    actual.put(node + 0x8c, destination);
    const auto previous = main_car_pose_cases::words(c.previous);
    for (unsigned i = 0; i < previous.size(); ++i) actual.put(destination + i * 4, previous[i]);
    actual.external = c.external;
    expected.ram = actual.ram; expected.external = actual.external;
    const auto before = actual.ram;
    const auto result = make_main_car_pose(c.state, c.external);
    const auto output = main_car_pose_cases::words(result);
    if (output != c.expected) throw rt::Fatal("native main pose differs from independent value contract");
    for (unsigned i = 0; i < output.size(); ++i) expected.put(destination + i * 4, output[i]);

    std::vector<Access> expected_reads{{node + 0x8c, destination}};
    for (unsigned i : {20u, 21u, 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
                       10u, 11u, 9u, 12u, 13u, 14u, 15u}) {
        const auto offset = source_offsets[i < 16 ? i : i - 4];
        expected_reads.push_back({node + offset, output[i]});
    }
    for (unsigned i = 0; i < 4; ++i) expected_reads.push_back({external_base + i * 4, c.external[i]});
    std::vector<Access> expected_writes;
    for (unsigned i : {20u, 21u, 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
                       10u, 11u, 9u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u})
        expected_writes.push_back({destination + i * 4, output[i]});

    rt::I960Ref reference(&actual);
    for (auto &word : reference.m_r) word = main_car_pose_cases::next(random_state);
    const auto ac = main_car_pose_cases::next(random_state);
    reference.m_AC = ac; reference.m_r[16] = node; reference.m_r[30] = sentinel;
    reference.m_IP = entry;
    unsigned steps = 0;
    while (reference.m_IP != sentinel) {
        if (++steps > 42) throw rt::Fatal("main pose helper exceeded original instruction bound");
        reference.execute_one();
    }
    if (steps != 42 || actual.ram != expected.ram || actual.external != expected.external ||
        actual.reads != expected_reads || actual.writes != expected_writes ||
        reference.m_AC != ac || reference.m_r[30] != 0)
        throw rt::Fatal("original main pose access/state differs from native contract");
    // This assertion separately binds all source and gap bytes, including the
    // source flags, against their initial state rather than a decoded result.
    for (unsigned i = 0; i < actual.ram.size(); ++i)
        if ((ram_base + i < destination || ram_base + i >= destination + 88) && actual.ram[i] != before[i])
            throw rt::Fatal("original helper modified non-destination storage");
    main_car_pose_cases::hash_words(fingerprint, output);
    distinct_inputs.insert(c.expected);
    ++cases; instructions += steps;
    return true;
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    try {
        std::ifstream stream(argv[1], std::ios::binary);
        const std::vector<std::uint8_t> program{std::istreambuf_iterator<char>(stream), {}};
        if (program.size() != 2097152) throw rt::Fatal("wrong private program size");
        if (!main_car_pose_cases::each([&](const auto &c) { return compare(program, c); }, true)) return 1;
        if (cases != 77208 || fingerprint != 0xb74eb51b13296a1cull)
            throw rt::Fatal("main pose original comparison coverage/fingerprint changed");
        std::printf("MAIN_CAR_POSE_ORACLE PASS cases=%u unique_inputs=%zu instructions=%u reads=%u writes=%u hash=%016llx\n",
                    cases, distinct_inputs.size(), instructions, cases * 23, cases * 22,
                    static_cast<unsigned long long>(fingerprint));
    } catch (const std::exception &error) {
        std::fprintf(stderr, "main pose oracle failed: %s\n", error.what()); return 1;
    }
    return 0;
}
